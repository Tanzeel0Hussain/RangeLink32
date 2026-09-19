#include <Preferences.h>
#include "storage.h"
#include "config.h"

namespace {
Preferences prefs;
constexpr uint8_t MAX_EVENT_LOGS = 20;
uint32_t bootSequence = 0;

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", " ");
  value.replace("\r", " ");
  return value;
}

String logKey(size_t index) {
  return "l" + String(index);
}

String wifiKey(size_t index, const char* suffix) {
  return "w" + String(index) + suffix;
}

String clientKey(size_t index, const char* suffix) {
  return "c" + String(index) + suffix;
}

void writeWifiSlot(size_t index, const WifiProfile& profile) {
  prefs.putString(wifiKey(index, "s").c_str(), profile.ssid);
  prefs.putString(wifiKey(index, "p").c_str(), profile.secret);
  prefs.putInt(wifiKey(index, "q").c_str(), profile.priority);
  prefs.putInt(wifiKey(index, "r").c_str(), profile.lastRssi);
  prefs.putBool(wifiKey(index, "e").c_str(), profile.enabled);
}

void clearWifiSlot(size_t index) {
  prefs.remove(wifiKey(index, "s").c_str());
  prefs.remove(wifiKey(index, "p").c_str());
  prefs.remove(wifiKey(index, "q").c_str());
  prefs.remove(wifiKey(index, "r").c_str());
  prefs.remove(wifiKey(index, "e").c_str());
}

void writeClientSlot(size_t index, const ClientRecord& record) {
  prefs.putString(clientKey(index, "m").c_str(), record.mac);
  prefs.putString(clientKey(index, "n").c_str(), record.hostname);
  prefs.putBool(clientKey(index, "a").c_str(), record.approved);
  prefs.putBool(clientKey(index, "b").c_str(), record.blocked);

  prefs.putULong64(clientKey(index, "rx").c_str(), record.rxBytes);
  prefs.putULong64(clientKey(index, "tx").c_str(), record.txBytes);
  prefs.putULong64(clientKey(index, "dr").c_str(), record.dailyRxBytes);
  prefs.putULong64(clientKey(index, "dt").c_str(), record.dailyTxBytes);
  prefs.putULong64(clientKey(index, "q").c_str(), record.dailyQuotaBytes);

  prefs.putUInt(clientKey(index, "bw").c_str(), record.bandwidthKbps);
  prefs.putUInt(clientKey(index, "gu").c_str(), record.guestUntilEpoch);
  prefs.putInt(clientKey(index, "day").c_str(), record.usageDay);

  prefs.putBool(clientKey(index, "se").c_str(), record.scheduleEnabled);
  prefs.putUChar(clientKey(index, "sh").c_str(), record.scheduleStartHour);
  prefs.putUChar(clientKey(index, "eh").c_str(), record.scheduleEndHour);
}
}

void storageBegin() {
  prefs.begin("rangelink32", false);

  bootSequence = prefs.getUInt("boot_seq", 0) + 1;
  prefs.putUInt("boot_seq", bootSequence);

  if (!prefs.isKey("ap_ssid")) {
    prefs.putString("ap_ssid", RangeLinkConfig::DEFAULT_AP_SSID);
  }
  if (!prefs.isKey("ap_pass")) {
    prefs.putString("ap_pass", RangeLinkConfig::DEFAULT_AP_PASSWORD);
  }
  if (!prefs.isKey("admin_user")) {
    prefs.putString("admin_user", RangeLinkConfig::DEFAULT_ADMIN_USER);
  }
  if (!prefs.isKey("admin_pass")) {
    prefs.putString("admin_pass", RangeLinkConfig::DEFAULT_ADMIN_PASSWORD);
  }
  if (!prefs.isKey("tz_min")) {
    prefs.putInt("tz_min", 300);
  }
}

String getApSsid() {
  return prefs.getString("ap_ssid", RangeLinkConfig::DEFAULT_AP_SSID);
}

String getApPassword() {
  return prefs.getString("ap_pass", RangeLinkConfig::DEFAULT_AP_PASSWORD);
}

String getAdminUser() {
  return prefs.getString("admin_user", RangeLinkConfig::DEFAULT_ADMIN_USER);
}

String getAdminPassword() {
  return prefs.getString("admin_pass", RangeLinkConfig::DEFAULT_ADMIN_PASSWORD);
}

bool setApCredentials(const String& ssid, const String& password) {
  if (ssid.length() == 0 || password.length() < 8) return false;
  prefs.putString("ap_ssid", ssid);
  prefs.putString("ap_pass", password);
  return true;
}

bool setAdminCredentials(const String& username, const String& password) {
  if (username.length() == 0 || password.length() < 8) return false;
  prefs.putString("admin_user", username);
  prefs.putString("admin_pass", password);
  return true;
}

size_t loadWifiProfiles(WifiProfile* out, size_t maxCount) {
  if (!out || maxCount == 0) return 0;

  size_t count = prefs.getUChar("wifi_n", 0);
  if (count > RangeLinkConfig::MAX_WIFI_PROFILES) {
    count = RangeLinkConfig::MAX_WIFI_PROFILES;
  }
  if (count > maxCount) count = maxCount;

  size_t written = 0;

  for (size_t i = 0; i < count; ++i) {
    WifiProfile profile;

    profile.ssid =
      prefs.getString(wifiKey(i, "s").c_str(), "");
    profile.secret =
      prefs.getString(wifiKey(i, "p").c_str(), "");
    profile.priority =
      prefs.getInt(wifiKey(i, "q").c_str(), 100);
    profile.lastRssi =
      prefs.getInt(wifiKey(i, "r").c_str(), -127);
    profile.enabled =
      prefs.getBool(wifiKey(i, "e").c_str(), true);

    if (profile.ssid.length() == 0) continue;
    out[written++] = profile;
  }

  return written;
}

bool saveWifiProfile(const WifiProfile& profile) {
  if (profile.ssid.length() == 0 || profile.secret.length() < 8) {
    return false;
  }

  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  for (size_t i = 0; i < count; ++i) {
    if (profiles[i].ssid == profile.ssid) {
      WifiProfile updated = profile;
      updated.priority = profiles[i].priority;
      updated.rxBytes = profiles[i].rxBytes;
      updated.txBytes = profiles[i].txBytes;
      writeWifiSlot(i, updated);
      return true;
    }
  }

  if (count >= RangeLinkConfig::MAX_WIFI_PROFILES) return false;

  WifiProfile stored = profile;
  stored.priority = 100 + static_cast<int>(count);

  writeWifiSlot(count, stored);
  prefs.putUChar("wifi_n", static_cast<uint8_t>(count + 1));

  return true;
}

bool removeWifiProfile(const String& ssid) {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  size_t found = count;

  for (size_t i = 0; i < count; ++i) {
    if (profiles[i].ssid == ssid) {
      found = i;
      break;
    }
  }

  if (found == count) return false;

  for (size_t i = found; i + 1 < count; ++i) {
    writeWifiSlot(i, profiles[i + 1]);
  }

  if (count > 0) {
    clearWifiSlot(count - 1);
    prefs.putUChar("wifi_n", static_cast<uint8_t>(count - 1));
  }

  return true;
}

uint8_t getStoredAccessMode() {
  return prefs.getUChar("access_mode", 0);
}

void setStoredAccessMode(uint8_t mode) {
  prefs.putUChar("access_mode", mode);
}

size_t loadClientPolicies(ClientRecord* out, size_t maxCount) {
  if (!out || maxCount == 0) return 0;

  size_t count = prefs.getUChar("client_n", 0);

  if (count > RangeLinkConfig::MAX_CLIENT_RECORDS) {
    count = RangeLinkConfig::MAX_CLIENT_RECORDS;
  }
  if (count > maxCount) count = maxCount;

  size_t written = 0;

  for (size_t i = 0; i < count; ++i) {
    ClientRecord record;

    record.mac =
      prefs.getString(clientKey(i, "m").c_str(), "");
    record.hostname =
      prefs.getString(clientKey(i, "n").c_str(), "");
    record.approved =
      prefs.getBool(clientKey(i, "a").c_str(), false);
    record.blocked =
      prefs.getBool(clientKey(i, "b").c_str(), false);

    record.rxBytes =
      prefs.getULong64(clientKey(i, "rx").c_str(), 0);
    record.txBytes =
      prefs.getULong64(clientKey(i, "tx").c_str(), 0);
    record.dailyRxBytes =
      prefs.getULong64(clientKey(i, "dr").c_str(), 0);
    record.dailyTxBytes =
      prefs.getULong64(clientKey(i, "dt").c_str(), 0);
    record.dailyQuotaBytes =
      prefs.getULong64(clientKey(i, "q").c_str(), 0);

    record.bandwidthKbps =
      prefs.getUInt(clientKey(i, "bw").c_str(), 0);
    record.guestUntilEpoch =
      prefs.getUInt(clientKey(i, "gu").c_str(), 0);
    record.usageDay =
      prefs.getInt(clientKey(i, "day").c_str(), -1);

    record.scheduleEnabled =
      prefs.getBool(clientKey(i, "se").c_str(), false);
    record.scheduleStartHour =
      prefs.getUChar(clientKey(i, "sh").c_str(), 0);
    record.scheduleEndHour =
      prefs.getUChar(clientKey(i, "eh").c_str(), 24);

    if (record.mac.length() == 0) continue;
    out[written++] = record;
  }

  return written;
}

bool saveClientPolicy(const ClientRecord& record) {
  if (record.mac.length() != 17) return false;

  ClientRecord policies[RangeLinkConfig::MAX_CLIENT_RECORDS];
  size_t count =
    loadClientPolicies(policies, RangeLinkConfig::MAX_CLIENT_RECORDS);

  for (size_t i = 0; i < count; ++i) {
    if (policies[i].mac.equalsIgnoreCase(record.mac)) {
      writeClientSlot(i, record);
      return true;
    }
  }

  if (count >= RangeLinkConfig::MAX_CLIENT_RECORDS) return false;

  writeClientSlot(count, record);
  prefs.putUChar("client_n", static_cast<uint8_t>(count + 1));

  return true;
}

int getTimezoneOffsetMinutes() {
  return prefs.getInt("tz_min", 300);
}

void setTimezoneOffsetMinutes(int minutes) {
  if (minutes < -720) minutes = -720;
  if (minutes > 840) minutes = 840;
  prefs.putInt("tz_min", minutes);
}

void appendEventLog(const String& type, const String& message) {
  uint8_t head = prefs.getUChar("log_head", 0);
  uint8_t count = prefs.getUChar("log_count", 0);

  String safeType = type;
  String safeMessage = message;

  safeType.replace("|", "/");
  safeMessage.replace("|", "/");
  safeMessage.replace("\n", " ");
  safeMessage.replace("\r", " ");

  String entry =
    String(bootSequence) + "|" +
    String(millis() / 1000UL) + "|" +
    safeType + "|" +
    safeMessage;

  prefs.putString(logKey(head).c_str(), entry);

  head =
    static_cast<uint8_t>((head + 1) % MAX_EVENT_LOGS);

  if (count < MAX_EVENT_LOGS) ++count;

  prefs.putUChar("log_head", head);
  prefs.putUChar("log_count", count);
}

String getEventLogJson() {
  const uint8_t head =
    prefs.getUChar("log_head", 0);
  const uint8_t count =
    prefs.getUChar("log_count", 0);

  String json = "[";

  const uint8_t oldest =
    static_cast<uint8_t>(
      (head + MAX_EVENT_LOGS - count) % MAX_EVENT_LOGS
    );

  for (uint8_t n = 0; n < count; ++n) {
    const uint8_t index =
      static_cast<uint8_t>((oldest + n) % MAX_EVENT_LOGS);

    String entry =
      prefs.getString(logKey(index).c_str(), "");

    if (entry.length() == 0) continue;

    const int p1 = entry.indexOf('|');
    const int p2 = entry.indexOf('|', p1 + 1);
    const int p3 = entry.indexOf('|', p2 + 1);

    if (p1 < 0 || p2 < 0 || p3 < 0) continue;

    if (json.length() > 1) json += ",";

    const String boot = entry.substring(0, p1);
    const String seconds = entry.substring(p1 + 1, p2);
    const String type = entry.substring(p2 + 1, p3);
    const String message = entry.substring(p3 + 1);

    json += "{\"boot\":" + boot +
            ",\"seconds\":" + seconds +
            ",\"type\":\"" + jsonEscape(type) +
            "\",\"message\":\"" + jsonEscape(message) +
            "\"}";
  }

  json += "]";
  return json;
}

void clearEventLogs() {
  for (uint8_t i = 0; i < MAX_EVENT_LOGS; ++i) {
    prefs.remove(logKey(i).c_str());
  }

  prefs.putUChar("log_head", 0);
  prefs.putUChar("log_count", 0);
}

void factoryResetStorage() {
  prefs.clear();
}
