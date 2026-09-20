#include <Arduino.h>
#include <WiFi.h>
#include <cstring>
#include <ctime>

extern "C" {
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"
}

#include "access_control.h"
#include "storage.h"
#include "traffic_monitor.h"
#include "config.h"
#include "policy_logic.h"
#include "text_utils.h"

namespace {
constexpr unsigned long CLIENT_REFRESH_MS = 2000;
constexpr unsigned long STATS_FLUSH_MS = 120000;

AccessMode mode = AccessMode::AllowAll;

ClientRecord liveClients[RangeLinkConfig::MAX_CLIENT_RECORDS];
size_t liveCount = 0;

ClientRecord policies[RangeLinkConfig::MAX_CLIENT_RECORDS];
size_t policyCount = 0;

unsigned long lastRefreshMs = 0;
unsigned long lastStatsFlushMs = 0;

String macToString(const uint8_t mac[6]) {
  char out[18];

  snprintf(
    out,
    sizeof(out),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2],
    mac[3], mac[4], mac[5]
  );

  return String(out);
}

String jsonEscape(const String& value) {
  return RangeLinkText::jsonEscape(value);
}

int policyIndex(const String& mac) {
  for (size_t i = 0; i < policyCount; ++i) {
    if (policies[i].mac.equalsIgnoreCase(mac)) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void updateDefaultInternetFallback() {
  // Unknown/no-slot clients are allowed only while there is room
  // to persist and account for them. Once history is full, fail
  // closed so a 25th device cannot bypass quotas/accounting.
  trafficMonitorSetDefaultAllow(
    mode == AccessMode::AllowAll &&
    policyCount < RangeLinkConfig::MAX_CLIENT_RECORDS
  );
}

void reloadPolicies() {
  policyCount =
    loadClientPolicies(
      policies,
      RangeLinkConfig::MAX_CLIENT_RECORDS
    );

  updateDefaultInternetFallback();
}

uint32_t currentEpoch() {
  const time_t now = time(nullptr);

  if (now < 1700000000) return 0;
  return static_cast<uint32_t>(now);
}

int32_t currentLocalMonth() {
  const uint32_t epoch = currentEpoch();
  if (!epoch) return -1;

  const time_t shifted =
    static_cast<time_t>(
      static_cast<int64_t>(epoch) +
      static_cast<int64_t>(getTimezoneOffsetMinutes()) * 60LL
    );

  struct tm localTime = {};
  gmtime_r(&shifted, &localTime);

  return
    (localTime.tm_year + 1900) * 12 +
    localTime.tm_mon;
}

int32_t currentLocalDay() {
  const uint32_t epoch = currentEpoch();

  if (!epoch) return -1;

  const int64_t shifted =
    static_cast<int64_t>(epoch) +
    static_cast<int64_t>(getTimezoneOffsetMinutes()) * 60LL;

  return static_cast<int32_t>(shifted / 86400LL);
}

uint8_t currentLocalHour() {
  const uint32_t epoch = currentEpoch();

  if (!epoch) return 255;

  int64_t shifted =
    static_cast<int64_t>(epoch) +
    static_cast<int64_t>(getTimezoneOffsetMinutes()) * 60LL;

  int64_t secondsInDay = shifted % 86400LL;
  if (secondsInDay < 0) secondsInDay += 86400LL;

  return static_cast<uint8_t>(secondsInDay / 3600LL);
}

bool scheduleAllows(const ClientRecord& record) {
  return RangeLinkLogic::scheduleAllowsHour(
    record.scheduleEnabled,
    record.scheduleStartHour,
    record.scheduleEndHour,
    currentLocalHour()
  );
}

bool guestActive(const ClientRecord& record) {
  if (!record.guestUntilEpoch) return false;

  const uint32_t now = currentEpoch();
  return now && now < record.guestUntilEpoch;
}

void refreshStats(ClientRecord& record) {
  uint64_t rx = record.rxBytes;
  uint64_t tx = record.txBytes;
  uint64_t dailyRx = record.dailyRxBytes;
  uint64_t dailyTx = record.dailyTxBytes;
  uint64_t monthlyRx = record.monthlyRxBytes;
  uint64_t monthlyTx = record.monthlyTxBytes;

  if (
    trafficMonitorGetStats(
      record.mac,
      rx,
      tx,
      dailyRx,
      dailyTx,
      monthlyRx,
      monthlyTx
    )
  ) {
    record.rxBytes = rx;
    record.txBytes = tx;
    record.dailyRxBytes = dailyRx;
    record.dailyTxBytes = dailyTx;
    record.monthlyRxBytes = monthlyRx;
    record.monthlyTxBytes = monthlyTx;
  }
}

void resetMonthlyIfNeeded(ClientRecord& record) {
  const int32_t month = currentLocalMonth();

  if (month < 0) return;

  if (record.usageMonth < 0) {
    record.usageMonth = month;
    return;
  }

  if (record.usageMonth != month) {
    trafficMonitorResetMonthlyUsage(record.mac);

    record.monthlyRxBytes = 0;
    record.monthlyTxBytes = 0;
    record.usageMonth = month;

    appendEventLog(
      "quota",
      "Monthly usage reset for " + record.mac
    );
  }
}

void resetDailyIfNeeded(ClientRecord& record) {
  const int32_t day = currentLocalDay();

  if (day < 0) return;

  if (record.usageDay < 0) {
    record.usageDay = day;
    return;
  }

  if (record.usageDay != day) {
    trafficMonitorResetUsage(record.mac, false);

    record.dailyRxBytes = 0;
    record.dailyTxBytes = 0;
    record.usageDay = day;

    appendEventLog(
      "quota",
      "Daily usage reset for " + record.mac
    );
  }
}

bool effectivePolicyAllows(ClientRecord& record) {
  refreshStats(record);
  resetDailyIfNeeded(record);
  resetMonthlyIfNeeded(record);

  if (record.blocked) return false;

  const bool identityAllowed =
    mode == AccessMode::AllowAll ||
    record.approved ||
    guestActive(record);

  if (!identityAllowed) return false;
  if (!scheduleAllows(record)) return false;

  if (
    RangeLinkLogic::quotaReached(
      record.dailyRxBytes,
      record.dailyTxBytes,
      record.dailyQuotaBytes
    )
  ) {
    return false;
  }

  if (
    RangeLinkLogic::quotaReached(
      record.monthlyRxBytes,
      record.monthlyTxBytes,
      record.monthlyQuotaBytes
    )
  ) {
    return false;
  }

  return true;
}

bool ipForMac(
  const wifi_sta_mac_ip_list_t& ipList,
  const uint8_t mac[6],
  String& out
) {
  for (int i = 0; i < ipList.num; ++i) {
    if (memcmp(ipList.sta[i].mac, mac, 6) == 0) {
      IPAddress ip(ipList.sta[i].ip.addr);
      out = ip.toString();
      return true;
    }
  }

  return false;
}

ClientRecord policyFor(const String& mac) {
  const int index = policyIndex(mac);

  if (index >= 0) {
    ClientRecord record = policies[index];
    refreshStats(record);
    resetDailyIfNeeded(record);
    return record;
  }

  ClientRecord record;
  record.mac = mac;
  record.usageDay = currentLocalDay();
  record.usageMonth = currentLocalMonth();

  return record;
}

bool runtimeStateChanged(
  const ClientRecord& current,
  const ClientRecord& persisted
) {
  return
    current.rxBytes != persisted.rxBytes ||
    current.txBytes != persisted.txBytes ||
    current.dailyRxBytes != persisted.dailyRxBytes ||
    current.dailyTxBytes != persisted.dailyTxBytes ||
    current.monthlyRxBytes != persisted.monthlyRxBytes ||
    current.monthlyTxBytes != persisted.monthlyTxBytes ||
    current.usageDay != persisted.usageDay ||
    current.usageMonth != persisted.usageMonth;
}

void persistRuntimeStats() {
  ClientRecord persisted[
    RangeLinkConfig::MAX_CLIENT_RECORDS
  ];

  const size_t persistedCount =
    loadClientPolicies(
      persisted,
      RangeLinkConfig::MAX_CLIENT_RECORDS
    );

  for (size_t i = 0; i < policyCount; ++i) {
    ClientRecord record = policies[i];

    refreshStats(record);
    resetDailyIfNeeded(record);
    resetMonthlyIfNeeded(record);

    const ClientRecord* saved = nullptr;

    for (size_t p = 0; p < persistedCount; ++p) {
      if (
        persisted[p].mac.equalsIgnoreCase(
          record.mac
        )
      ) {
        saved = &persisted[p];
        break;
      }
    }

    if (
      !saved ||
      runtimeStateChanged(record, *saved)
    ) {
      saveClientPolicy(record);
    }

    policies[i] = record;
  }

  lastStatsFlushMs = millis();
}

bool policyIsUnmanaged(
  const ClientRecord& record
) {
  return
    !record.approved &&
    !record.blocked &&
    record.hostname.length() == 0 &&
    record.dailyQuotaBytes == 0 &&
    record.monthlyQuotaBytes == 0 &&
    record.bandwidthKbps == 0 &&
    !record.scheduleEnabled &&
    record.guestUntilEpoch == 0;
}

bool macIsCurrentlyConnected(
  const wifi_sta_list_t& wifiList,
  const String& mac
) {
  for (int i = 0; i < wifiList.num; ++i) {
    if (
      macToString(wifiList.sta[i].mac)
        .equalsIgnoreCase(mac)
    ) {
      return true;
    }
  }

  return false;
}

bool reclaimUnmanagedClientSlot(
  const wifi_sta_list_t& wifiList
) {
  for (size_t i = 0; i < policyCount; ++i) {
    const ClientRecord& candidate = policies[i];

    if (
      policyIsUnmanaged(candidate) &&
      !macIsCurrentlyConnected(
        wifiList,
        candidate.mac
      )
    ) {
      const String removedMac =
        candidate.mac;

      if (!removeClientPolicy(removedMac)) {
        return false;
      }

      trafficMonitorForgetClient(removedMac);
      reloadPolicies();

      appendEventLog(
        "client",
        "Reclaimed unmanaged history slot " +
        removedMac
      );

      return true;
    }
  }

  return false;
}

void refreshClients() {
  wifi_sta_list_t wifiList = {};

  if (esp_wifi_ap_get_sta_list(&wifiList) != ESP_OK) {
    liveCount = 0;
    lastRefreshMs = millis();
    return;
  }

  wifi_sta_mac_ip_list_t ipList = {};

  const bool haveIpList =
    esp_wifi_ap_get_sta_list_with_ip(
      &wifiList,
      &ipList
    ) == ESP_OK;

  liveCount = 0;

  const size_t count =
    static_cast<size_t>(wifiList.num) >
      RangeLinkConfig::MAX_CLIENT_RECORDS
      ? RangeLinkConfig::MAX_CLIENT_RECORDS
      : static_cast<size_t>(wifiList.num);

  for (size_t i = 0; i < count; ++i) {
    ClientRecord record;

    record.mac =
      macToString(wifiList.sta[i].mac);

    record.rssi =
      wifiList.sta[i].rssi;

    record.connected = true;
    record.ip = "0.0.0.0";

    if (haveIpList) {
      ipForMac(
        ipList,
        wifiList.sta[i].mac,
        record.ip
      );
    }

    int index = policyIndex(record.mac);

    if (index < 0) {
      ClientRecord seen;
      seen.mac = record.mac;
      seen.usageDay = currentLocalDay();
      seen.usageMonth = currentLocalMonth();

      bool saved =
        saveClientPolicy(seen);

      if (
        !saved &&
        reclaimUnmanagedClientSlot(wifiList)
      ) {
        saved =
          saveClientPolicy(seen);
      }

      reloadPolicies();
      index = policyIndex(record.mac);

      appendEventLog(
        "client",
        saved
          ? "First seen " + record.mac
          : "Client history full; could not save " +
            record.mac
      );
    }

    if (index >= 0) {
      ClientRecord stored = policies[index];

      stored.ip = record.ip;
      stored.rssi = record.rssi;
      stored.connected = true;

      refreshStats(stored);
      resetDailyIfNeeded(stored);
      resetMonthlyIfNeeded(stored);

      const bool allowed =
        effectivePolicyAllows(stored);

      trafficMonitorConfigureClient(
        stored,
        allowed
      );

      liveClients[liveCount++] = stored;
    }
  }

  // Keep policies for offline/previously seen devices synchronized
  // with traffic data so history remains useful.
  for (size_t i = 0; i < policyCount; ++i) {
    ClientRecord stored = policies[i];

    refreshStats(stored);
    resetDailyIfNeeded(stored);
    resetMonthlyIfNeeded(stored);

    const bool allowed =
      effectivePolicyAllows(stored);

    trafficMonitorConfigureClient(
      stored,
      allowed
    );

    policies[i] = stored;
  }

  lastRefreshMs = millis();
}
}

void accessControlBegin() {
  const uint8_t stored = getStoredAccessMode();

  mode =
    stored ==
      static_cast<uint8_t>(AccessMode::AllowlistOnly)
      ? AccessMode::AllowlistOnly
      : AccessMode::AllowAll;

  reloadPolicies();
  refreshClients();

  appendEventLog(
    "access",
    mode == AccessMode::AllowlistOnly
      ? "Access mode: allowlist-only"
      : "Access mode: allow-all"
  );
}

void accessControlLoop() {
  if (
    millis() - lastRefreshMs >=
      CLIENT_REFRESH_MS
  ) {
    refreshClients();
  }

  if (
    millis() - lastStatsFlushMs >=
      STATS_FLUSH_MS
  ) {
    persistRuntimeStats();
  }
}

void accessControlFlush() {
  persistRuntimeStats();
}

bool accessControlTimeSynchronized() {
  return currentEpoch() != 0;
}

AccessMode getAccessMode() {
  return mode;
}

void setAccessMode(AccessMode newMode) {
  mode = newMode;

  setStoredAccessMode(
    static_cast<uint8_t>(newMode)
  );

  updateDefaultInternetFallback();

  appendEventLog(
    "access",
    newMode == AccessMode::AllowlistOnly
      ? "Access mode changed to allowlist-only"
      : "Access mode changed to allow-all"
  );

  refreshClients();
}

bool setClientApproval(
  const String& mac,
  bool approved
) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);
  record.mac = mac;
  record.approved = approved;

  if (!saveClientPolicy(record)) return false;

  appendEventLog(
    "client",
    String(
      approved
        ? "Approved "
        : "Removed approval for "
    ) + mac
  );

  reloadPolicies();
  refreshClients();

  return true;
}

bool setClientBlocked(
  const String& mac,
  bool blocked
) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);
  record.mac = mac;
  record.blocked = blocked;

  if (!saveClientPolicy(record)) return false;

  appendEventLog(
    "client",
    String(
      blocked
        ? "Blocked Internet for "
        : "Unblocked Internet for "
    ) + mac
  );

  reloadPolicies();
  refreshClients();

  return true;
}

bool setClientName(
  const String& mac,
  const String& name
) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);
  record.hostname = name.substring(0, 32);

  if (!saveClientPolicy(record)) return false;

  reloadPolicies();
  refreshClients();

  return true;
}

bool setClientLimits(
  const String& mac,
  uint64_t dailyQuotaBytes,
  uint64_t monthlyQuotaBytes,
  uint32_t bandwidthKbps
) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);

  record.dailyQuotaBytes = dailyQuotaBytes;
  record.monthlyQuotaBytes = monthlyQuotaBytes;
  record.bandwidthKbps = bandwidthKbps;

  if (!saveClientPolicy(record)) return false;

  appendEventLog(
    "client",
    "Updated quota/speed for " + mac
  );

  reloadPolicies();
  refreshClients();

  return true;
}

bool setClientSchedule(
  const String& mac,
  bool enabled,
  uint8_t startHour,
  uint8_t endHour
) {
  if (mac.length() != 17) return false;
  if (startHour > 23 || endHour > 24) return false;

  ClientRecord record = policyFor(mac);

  record.scheduleEnabled = enabled;
  record.scheduleStartHour = startHour;
  record.scheduleEndHour = endHour;

  if (!saveClientPolicy(record)) return false;

  appendEventLog(
    "client",
    "Updated schedule for " + mac
  );

  reloadPolicies();
  refreshClients();

  return true;
}

bool grantGuestAccess(
  const String& mac,
  uint32_t minutes
) {
  if (mac.length() != 17) return false;

  const uint32_t now = currentEpoch();
  if (!now) return false;

  ClientRecord record = policyFor(mac);

  record.guestUntilEpoch =
    now + minutes * 60UL;

  if (!saveClientPolicy(record)) return false;

  appendEventLog(
    "guest",
    "Granted " + String(minutes) +
    " minutes to " + mac
  );

  reloadPolicies();
  refreshClients();

  return true;
}

bool resetClientUsage(
  const String& mac,
  bool resetTotal
) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);

  trafficMonitorResetUsage(mac, resetTotal);

  record.dailyRxBytes = 0;
  record.dailyTxBytes = 0;
  record.usageDay = currentLocalDay();

  if (resetTotal) {
    trafficMonitorResetMonthlyUsage(mac);
    record.monthlyRxBytes = 0;
    record.monthlyTxBytes = 0;
    record.usageMonth = currentLocalMonth();
    record.rxBytes = 0;
    record.txBytes = 0;
  }

  if (!saveClientPolicy(record)) return false;

  appendEventLog(
    "client",
    String(
      resetTotal
        ? "Reset total usage for "
        : "Reset daily usage for "
    ) + mac
  );

  reloadPolicies();
  refreshClients();

  return true;
}

bool resetClientMonthlyUsage(
  const String& mac
) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);

  trafficMonitorResetMonthlyUsage(mac);

  record.monthlyRxBytes = 0;
  record.monthlyTxBytes = 0;
  record.usageMonth = currentLocalMonth();

  if (!saveClientPolicy(record)) return false;

  appendEventLog(
    "client",
    "Reset monthly usage for " + mac
  );

  reloadPolicies();
  refreshClients();

  return true;
}

bool forgetKnownClient(
  const String& mac
) {
  if (mac.length() != 17) return false;

  for (size_t i = 0; i < liveCount; ++i) {
    if (
      liveClients[i].mac.equalsIgnoreCase(mac) &&
      liveClients[i].connected
    ) {
      return false;
    }
  }

  if (!removeClientPolicy(mac)) return false;

  trafficMonitorForgetClient(mac);
  appendEventLog(
    "client",
    "Forgot known device " + mac
  );

  reloadPolicies();
  refreshClients();
  return true;
}

bool clientMayUseInternet(
  const String& mac
) {
  ClientRecord record = policyFor(mac);
  return effectivePolicyAllows(record);
}

String getClientTableJson() {
  if (
    millis() - lastRefreshMs >= 750
  ) {
    refreshClients();
  }

  String json;
  json.reserve(
    256 +
    policyCount * 430
  );
  json = "[";

  for (size_t p = 0; p < policyCount; ++p) {
    if (p) json += ",";

    ClientRecord record = policies[p];

    record.connected = false;
    record.ip = "";
    record.rssi = -127;

    for (size_t i = 0; i < liveCount; ++i) {
      if (
        liveClients[i].mac.equalsIgnoreCase(
          record.mac
        )
      ) {
        record.ip = liveClients[i].ip;
        record.rssi = liveClients[i].rssi;
        record.connected = true;
        break;
      }
    }

    refreshStats(record);
    resetDailyIfNeeded(record);
    resetMonthlyIfNeeded(record);

    const bool allowed =
      effectivePolicyAllows(record);

    const bool guest =
      guestActive(record);

    json +=
      "{\"hostname\":\"" +
      jsonEscape(record.hostname) +
      "\",\"ip\":\"" +
      jsonEscape(record.ip) +
      "\",\"mac\":\"" +
      jsonEscape(record.mac) +
      "\",\"rssi\":" +
      String(record.rssi) +
      ",\"connected\":" +
      String(record.connected ? "true" : "false") +
      ",\"approved\":" +
      String(record.approved ? "true" : "false") +
      ",\"blocked\":" +
      String(record.blocked ? "true" : "false") +
      ",\"allowed\":" +
      String(allowed ? "true" : "false") +
      ",\"guest\":" +
      String(guest ? "true" : "false") +
      ",\"guestUntil\":" +
      String(record.guestUntilEpoch) +
      ",\"rxBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.rxBytes
        )
      ) +
      ",\"txBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.txBytes
        )
      ) +
      ",\"dailyRxBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.dailyRxBytes
        )
      ) +
      ",\"dailyTxBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.dailyTxBytes
        )
      ) +
      ",\"dailyQuotaBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.dailyQuotaBytes
        )
      ) +
      ",\"monthlyRxBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.monthlyRxBytes
        )
      ) +
      ",\"monthlyTxBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.monthlyTxBytes
        )
      ) +
      ",\"monthlyQuotaBytes\":" +
      String(
        static_cast<unsigned long long>(
          record.monthlyQuotaBytes
        )
      ) +
      ",\"bandwidthKbps\":" +
      String(record.bandwidthKbps) +
      ",\"scheduleEnabled\":" +
      String(
        record.scheduleEnabled
          ? "true"
          : "false"
      ) +
      ",\"scheduleStart\":" +
      String(record.scheduleStartHour) +
      ",\"scheduleEnd\":" +
      String(record.scheduleEndHour) +
      "}";

    policies[p] = record;
  }

  return json + "]";
}
