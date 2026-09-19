#include <WiFi.h>
#include "wifi_manager.h"
#include "storage.h"
#include "config.h"

namespace {
constexpr size_t MAX_SCAN_RESULTS = 32;
constexpr uint8_t FAILOVER_AFTER_ATTEMPTS = 4;
constexpr unsigned long HEALTH_CHECK_INTERVAL_MS = 30000;

SystemState state;

String targetSsid;
String targetPassword;

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

String scanJson = "[]";

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

  WiFiClient probe;
  IPAddress endpoint(1, 1, 1, 1);
  const bool online = probe.connect(endpoint, 443, 1000);
  if (online) probe.stop();

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

void startTarget(const String& ssid, const String& password) {
  targetSsid = ssid;
  targetPassword = password;

  reconnectAttempts = 0;
  reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
  lastReconnectAttempt = millis();

  WiFi.disconnect();
  WiFi.begin(targetSsid.c_str(), targetPassword.c_str());

  Serial.print("RangeLink32 upstream target: ");
  Serial.println(targetSsid);
  appendEventLog("upstream", "Connecting to " + targetSsid);
}

bool selectBestSavedProfile(bool preferDifferent) {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  const size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  int bestIndex = -1;
  int bestPriority = 32767;
  int32_t bestRssi = -128;

  for (size_t i = 0; i < count; ++i) {
    if (!profiles[i].enabled || profiles[i].secret.length() < 8) continue;

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

  startTarget(profiles[bestIndex].ssid, profiles[bestIndex].secret);
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
  WiFi.begin(targetSsid.c_str(), targetPassword.c_str());

  const unsigned long next = reconnectDelayMs * 2UL;
  reconnectDelayMs =
    next > static_cast<unsigned long>(RangeLinkConfig::RECONNECT_MAX_MS) ?
    static_cast<unsigned long>(RangeLinkConfig::RECONNECT_MAX_MS) : next;
}
}

void wifiManagerBegin() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  IPAddress ip(
    RangeLinkConfig::AP_IP_A,
    RangeLinkConfig::AP_IP_B,
    RangeLinkConfig::AP_IP_C,
    RangeLinkConfig::AP_IP_D
  );
  IPAddress mask(255, 255, 255, 0);

  WiFi.softAPConfig(ip, ip, mask);
  WiFi.softAP(getApSsid().c_str(), getApPassword().c_str());

  performScan();
  selectBestSavedProfile(false);
  refreshState();
  appendEventLog("system", "RangeLink32 Wi-Fi manager started");
}

void wifiManagerLoop() {
  maintainUpstream();
  refreshState();
  checkInternetHealth();
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
            ",\"current\":" +
            String(WiFi.status() == WL_CONNECTED &&
                   WiFi.SSID() == profiles[i].ssid ? "true" : "false") +
            "}";
  }
  json += "]";
  return json;
}

SystemState getSystemState() {
  refreshState();
  return state;
}

bool connectUpstream(const String& ssid, const String& password) {
  if (ssid.length() == 0 || password.length() < 8) return false;

  WifiProfile profile;
  profile.ssid = ssid;
  profile.secret = password;
  profile.enabled = true;

  if (!saveWifiProfile(profile)) {
    return false;
  }

  startTarget(ssid, password);
  return true;
}

bool connectSavedProfile(const String& ssid) {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  const size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  for (size_t i = 0; i < count; ++i) {
    if (profiles[i].ssid == ssid &&
        profiles[i].enabled &&
        profiles[i].secret.length() >= 8) {
      startTarget(profiles[i].ssid, profiles[i].secret);
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
