#include <WiFi.h>
#include "wifi_manager.h"
#include "storage.h"
#include "traffic_monitor.h"
#include "config.h"

namespace {
constexpr size_t MAX_SCAN_RESULTS = 32;
constexpr uint8_t FAILOVER_AFTER_ATTEMPTS = 4;
constexpr unsigned long HEALTH_CHECK_INTERVAL_MS = 30000;
constexpr unsigned long NETWORK_USAGE_FLUSH_MS = 60000;

SystemState state;

String targetSsid;
String targetPassword;
bool targetOpenNetwork = false;

String scanSsids[MAX_SCAN_RESULTS];
int32_t scanRssi[MAX_SCAN_RESULTS];
uint8_t scanChannels[MAX_SCAN_RESULTS];
bool scanSecure[MAX_SCAN_RESULTS];
size_t scanCount = 0;

unsigned long lastReconnectAttempt = 0;
unsigned long reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
unsigned long lastScanMs = 0;
uint8_t reconnectAttempts = 0;
unsigned long lastHealthCheckMs = 0;
bool lastUpstreamState = false;
bool upstreamStateInitialized = false;
unsigned long lastNetworkUsageFlushMs = 0;
uint64_t lastGatewayRxBytes = 0;
uint64_t lastGatewayTxBytes = 0;
String usageSsid;
String appliedDownstreamDns;

String scanJson = "[]";

IPAddress desiredDownstreamDns() {
  const String customDns = getCustomDns();

  if (customDns.length()) {
    IPAddress parsed;
    if (parsed.fromString(customDns)) {
      return parsed;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    const IPAddress upstreamDns =
      WiFi.dnsIP(0);

    if (
      static_cast<uint32_t>(
        upstreamDns
      ) != 0
    ) {
      return upstreamDns;
    }
  }

  return IPAddress(1, 1, 1, 1);
}

void applyDownstreamDnsIfNeeded() {
  const IPAddress dns =
    desiredDownstreamDns();

  const String dnsText =
    dns.toString();

  if (dnsText == appliedDownstreamDns) {
    return;
  }

  IPAddress ip(
    RangeLinkConfig::AP_IP_A,
    RangeLinkConfig::AP_IP_B,
    RangeLinkConfig::AP_IP_C,
    RangeLinkConfig::AP_IP_D
  );
  IPAddress mask(255, 255, 255, 0);
  IPAddress leaseStart(
    RangeLinkConfig::AP_IP_A,
    RangeLinkConfig::AP_IP_B,
    RangeLinkConfig::AP_IP_C,
    10
  );

  if (
    WiFi.AP.config(
      ip,
      ip,
      mask,
      leaseStart,
      dns
    )
  ) {
    appliedDownstreamDns = dnsText;

    appendEventLog(
      "dns",
      "Downstream DNS set to " +
      dnsText
    );
  }
}

void refreshState() {
  state.upstreamConnected = WiFi.status() == WL_CONNECTED;
  state.upstreamSsid = state.upstreamConnected ? WiFi.SSID() : "";
  state.upstreamRssi = state.upstreamConnected ? WiFi.RSSI() : -127;
  state.apSsid = getApSsid();
  state.connectedClients = WiFi.softAPgetStationNum();

  if (!state.upstreamConnected) {
    state.internetReachable = false;
  }

  if (!upstreamStateInitialized) {
    lastUpstreamState = state.upstreamConnected;
    upstreamStateInitialized = true;
  } else if (lastUpstreamState != state.upstreamConnected) {
    if (state.upstreamConnected) {
      appendEventLog(
        "upstream",
        "Connected to " + state.upstreamSsid +
        " at " + String(state.upstreamRssi) + " dBm"
      );
    } else {
      appendEventLog("upstream", "Upstream Wi-Fi disconnected");
    }
    lastUpstreamState = state.upstreamConnected;
  }

  applyDownstreamDnsIfNeeded();
}

void accountNetworkUsage() {
  uint64_t gatewayRx = 0;
  uint64_t gatewayTx = 0;

  trafficMonitorGetTotals(
    gatewayRx,
    gatewayTx
  );

  if (!state.upstreamConnected) {
    lastGatewayRxBytes = gatewayRx;
    lastGatewayTxBytes = gatewayTx;
    usageSsid = "";
    return;
  }

  if (usageSsid != state.upstreamSsid) {
    usageSsid = state.upstreamSsid;
    lastGatewayRxBytes = gatewayRx;
    lastGatewayTxBytes = gatewayTx;
    lastNetworkUsageFlushMs = millis();
    return;
  }

  if (
    millis() - lastNetworkUsageFlushMs <
      NETWORK_USAGE_FLUSH_MS
  ) {
    return;
  }

  const uint64_t rxDelta =
    gatewayRx >= lastGatewayRxBytes
      ? gatewayRx - lastGatewayRxBytes
      : 0;

  const uint64_t txDelta =
    gatewayTx >= lastGatewayTxBytes
      ? gatewayTx - lastGatewayTxBytes
      : 0;

  if (rxDelta || txDelta) {
    updateWifiProfileUsage(
      state.upstreamSsid,
      state.upstreamRssi,
      rxDelta,
      txDelta
    );
  }

  lastGatewayRxBytes = gatewayRx;
  lastGatewayTxBytes = gatewayTx;
  lastNetworkUsageFlushMs = millis();
}

void checkInternetHealth() {
  if (!state.upstreamConnected) {
    state.internetReachable = false;
    return;
  }

  if (millis() - lastHealthCheckMs < HEALTH_CHECK_INTERVAL_MS) {
    return;
  }

  lastHealthCheckMs = millis();
  const bool previous = state.internetReachable;

  struct HealthTarget {
    IPAddress ip;
    uint16_t port;
  };

  const HealthTarget targets[] = {
    {IPAddress(1, 1, 1, 1), 443},
    {IPAddress(8, 8, 8, 8), 53},
    {IPAddress(9, 9, 9, 9), 53}
  };

  bool online = false;

  for (const HealthTarget& target : targets) {
    WiFiClient probe;

    if (
      probe.connect(
        target.ip,
        target.port,
        700
      )
    ) {
      probe.stop();
      online = true;
      break;
    }

    probe.stop();
  }

  state.internetReachable = online;

  if (online != previous) {
    appendEventLog(
      "internet",
      online ? "Internet health check passed" : "Internet health check failed"
    );
  }
}

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  return value;
}

int32_t scannedRssiFor(const String& ssid) {
  int32_t best = -127;
  for (size_t i = 0; i < scanCount; ++i) {
    if (scanSsids[i] == ssid && scanRssi[i] > best) {
      best = scanRssi[i];
    }
  }
  return best;
}

void performScan() {
  const int found = WiFi.scanNetworks(false, true);

  scanCount = 0;
  String json = "[";

  if (found > 0) {
    const size_t limit =
      static_cast<size_t>(found) > MAX_SCAN_RESULTS ?
      MAX_SCAN_RESULTS : static_cast<size_t>(found);

    for (size_t i = 0; i < limit; ++i) {
      scanSsids[scanCount] = WiFi.SSID(static_cast<int>(i));
      scanRssi[scanCount] = WiFi.RSSI(static_cast<int>(i));
      scanChannels[scanCount] =
        static_cast<uint8_t>(WiFi.channel(static_cast<int>(i)));
      scanSecure[scanCount] =
        WiFi.encryptionType(static_cast<int>(i)) != WIFI_AUTH_OPEN;

      if (scanCount) json += ",";
      json += "{\"ssid\":\"" + jsonEscape(scanSsids[scanCount]) +
              "\",\"rssi\":" + String(scanRssi[scanCount]) +
              ",\"channel\":" + String(scanChannels[scanCount]) +
              ",\"secure\":" +
              String(scanSecure[scanCount] ? "true" : "false") +
              "}";

      ++scanCount;
    }
  }

  json += "]";
  scanJson = json;
  WiFi.scanDelete();
  lastScanMs = millis();
}

void startTarget(
  const String& ssid,
  const String& password,
  bool openNetwork
) {
  targetSsid = ssid;
  targetPassword =
    openNetwork ? "" : password;
  targetOpenNetwork = openNetwork;

  reconnectAttempts = 0;
  reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
  lastReconnectAttempt = millis();

  WiFi.disconnect();

  if (targetOpenNetwork) {
    WiFi.begin(targetSsid.c_str());
  } else {
    WiFi.begin(
      targetSsid.c_str(),
      targetPassword.c_str()
    );
  }

  Serial.print("RangeLink32 upstream target: ");
  Serial.println(targetSsid);
  appendEventLog(
    "upstream",
    "Connecting to " + targetSsid +
    (targetOpenNetwork ? " (open)" : "")
  );
}

bool selectBestSavedProfile(bool preferDifferent) {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  const size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  int bestIndex = -1;
  int bestPriority = 32767;
  int32_t bestRssi = -128;

  for (size_t i = 0; i < count; ++i) {
    if (
      !profiles[i].enabled ||
      (!profiles[i].openNetwork &&
       profiles[i].secret.length() < 8)
    ) continue;

    const int32_t rssi = scannedRssiFor(profiles[i].ssid);
    if (rssi <= -127) continue;

    if (preferDifferent &&
        profiles[i].ssid == targetSsid &&
        count > 1) {
      continue;
    }

    if (bestIndex < 0 ||
        profiles[i].priority < bestPriority ||
        (profiles[i].priority == bestPriority && rssi > bestRssi)) {
      bestIndex = static_cast<int>(i);
      bestPriority = profiles[i].priority;
      bestRssi = rssi;
    }
  }

  if (bestIndex < 0 && preferDifferent) {
    return selectBestSavedProfile(false);
  }

  if (bestIndex < 0) return false;

  if (preferDifferent) {
    appendEventLog(
      "failover",
      "Switching to saved network " + profiles[bestIndex].ssid
    );
  }

  startTarget(
    profiles[bestIndex].ssid,
    profiles[bestIndex].secret,
    profiles[bestIndex].openNetwork
  );
  return true;
}

void maintainUpstream() {
  if (WiFi.status() == WL_CONNECTED) {
    reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
    reconnectAttempts = 0;
    return;
  }

  if (targetSsid.length() == 0) {
    if (millis() - lastScanMs >= RangeLinkConfig::SCAN_INTERVAL_MS) {
      performScan();
      selectBestSavedProfile(false);
    }
    return;
  }

  if (millis() - lastReconnectAttempt < reconnectDelayMs) return;

  if (reconnectAttempts >= FAILOVER_AFTER_ATTEMPTS) {
    performScan();

    if (selectBestSavedProfile(true)) {
      return;
    }

    reconnectAttempts = 0;
  }

  lastReconnectAttempt = millis();
  ++reconnectAttempts;

  Serial.print("RangeLink32 reconnect attempt ");
  Serial.print(reconnectAttempts);
  Serial.print(" for ");
  Serial.println(targetSsid);

  WiFi.disconnect();

  if (targetOpenNetwork) {
    WiFi.begin(targetSsid.c_str());
  } else {
    WiFi.begin(
      targetSsid.c_str(),
      targetPassword.c_str()
    );
  }

  const unsigned long next = reconnectDelayMs * 2UL;
  reconnectDelayMs =
    next > static_cast<unsigned long>(RangeLinkConfig::RECONNECT_MAX_MS) ?
    static_cast<unsigned long>(RangeLinkConfig::RECONNECT_MAX_MS) : next;
}
}

void wifiManagerBegin() {
  configTime(
    getTimezoneOffsetMinutes() * 60,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  IPAddress ip(
    RangeLinkConfig::AP_IP_A,
    RangeLinkConfig::AP_IP_B,
    RangeLinkConfig::AP_IP_C,
    RangeLinkConfig::AP_IP_D
  );
  IPAddress mask(255, 255, 255, 0);
  IPAddress leaseStart(
    RangeLinkConfig::AP_IP_A,
    RangeLinkConfig::AP_IP_B,
    RangeLinkConfig::AP_IP_C,
    10
  );

  const IPAddress dns =
    desiredDownstreamDns();

  WiFi.AP.begin();
  WiFi.AP.config(
    ip,
    ip,
    mask,
    leaseStart,
    dns
  );
  appliedDownstreamDns =
    dns.toString();
  WiFi.AP.create(
    getApSsid(),
    getApPassword(),
    1,
    0,
    8
  );

  performScan();
  selectBestSavedProfile(false);
  refreshState();
  appendEventLog("system", "RangeLink32 Wi-Fi manager started");
}

void wifiManagerLoop() {
  maintainUpstream();
  refreshState();
  checkInternetHealth();
  accountNetworkUsage();
}

void requestWifiScan() {
  performScan();
}

String getWifiScanJson() {
  return scanJson;
}

String getSavedProfilesJson() {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  const size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  String json = "[";
  for (size_t i = 0; i < count; ++i) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(profiles[i].ssid) +
            "\",\"priority\":" + String(profiles[i].priority) +
            ",\"enabled\":" +
            String(profiles[i].enabled ? "true" : "false") +
            ",\"open\":" +
            String(profiles[i].openNetwork ? "true" : "false") +
            ",\"current\":" +
            String(WiFi.status() == WL_CONNECTED &&
                   WiFi.SSID() == profiles[i].ssid ? "true" : "false") +
            ",\"lastRssi\":" + String(profiles[i].lastRssi) +
            ",\"rxBytes\":" +
            String(static_cast<unsigned long long>(profiles[i].rxBytes)) +
            ",\"txBytes\":" +
            String(static_cast<unsigned long long>(profiles[i].txBytes)) +
            "}";
  }
  json += "]";
  return json;
}

String getChannelAnalysisJson() {
  int score[14] = {};
  int networks[14] = {};

  for (size_t i = 0; i < scanCount; ++i) {
    const uint8_t channel = scanChannels[i];

    if (channel < 1 || channel > 13) {
      continue;
    }

    const int strength =
      scanRssi[i] >= -55 ? 4 :
      scanRssi[i] >= -67 ? 3 :
      scanRssi[i] >= -75 ? 2 : 1;

    for (int ch = 1; ch <= 13; ++ch) {
      const int distance =
        abs(ch - static_cast<int>(channel));

      if (distance <= 2) {
        score[ch] +=
          strength * (3 - distance);
      }
    }

    networks[channel]++;
  }

  String json = "[";
  for (int ch = 1; ch <= 13; ++ch) {
    if (ch > 1) json += ",";

    json +=
      "{\"channel\":" + String(ch) +
      ",\"networks\":" +
      String(networks[ch]) +
      ",\"congestion\":" +
      String(score[ch]) +
      "}";
  }

  return json + "]";
}

SystemState getSystemState() {
  refreshState();
  return state;
}

bool connectUpstream(
  const String& ssid,
  const String& password,
  bool openNetwork
) {
  if (
    ssid.length() == 0 ||
    ssid.length() > 32 ||
    (!openNetwork &&
      (password.length() < 8 ||
       password.length() > 63))
  ) {
    return false;
  }

  WifiProfile profile;
  profile.ssid = ssid;
  profile.secret =
    openNetwork ? "" : password;
  profile.enabled = true;
  profile.openNetwork = openNetwork;

  if (!saveWifiProfile(profile)) {
    return false;
  }

  startTarget(
    ssid,
    profile.secret,
    openNetwork
  );
  return true;
}

bool connectSavedProfile(const String& ssid) {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  const size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  for (size_t i = 0; i < count; ++i) {
    if (
      profiles[i].ssid == ssid &&
      profiles[i].enabled &&
      (profiles[i].openNetwork ||
       profiles[i].secret.length() >= 8)
    ) {
      startTarget(
        profiles[i].ssid,
        profiles[i].secret,
        profiles[i].openNetwork
      );
      return true;
    }
  }

  return false;
}

bool forgetSavedProfile(const String& ssid) {
  if (!removeWifiProfile(ssid)) return false;
  appendEventLog("profile", "Forgot saved network " + ssid);

  if (targetSsid == ssid) {
    targetSsid = "";
    targetPassword = "";
    targetOpenNetwork = false;
    WiFi.disconnect();
    performScan();
    selectBestSavedProfile(false);
  }

  return true;
}

void reconnectUpstream() {
  WiFi.disconnect();
  lastReconnectAttempt = 0;
  reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
}

bool setAccessPointCredentials(const String& ssid, const String& password) {
  if (!setApCredentials(ssid, password)) return false;

  WiFi.softAPdisconnect(true);
  delay(100);

  IPAddress ip(
    RangeLinkConfig::AP_IP_A,
    RangeLinkConfig::AP_IP_B,
    RangeLinkConfig::AP_IP_C,
    RangeLinkConfig::AP_IP_D
  );
  IPAddress mask(255, 255, 255, 0);

  WiFi.softAPConfig(ip, ip, mask);
  return WiFi.softAP(ssid.c_str(), password.c_str());
}
