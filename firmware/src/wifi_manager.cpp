#include <WiFi.h>
#include "wifi_manager.h"
#include "storage.h"
#include "config.h"

namespace {
SystemState state;

String targetSsid;
String targetPassword;

unsigned long lastReconnectAttempt = 0;
unsigned long reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
unsigned long lastScanMs = 0;

String scanJson = "[]";

void refreshState() {
  state.upstreamConnected = WiFi.status() == WL_CONNECTED;
  state.upstreamSsid = state.upstreamConnected ? WiFi.SSID() : "";
  state.upstreamRssi = state.upstreamConnected ? WiFi.RSSI() : -127;
  state.apSsid = getApSsid();
  state.connectedClients = WiFi.softAPgetStationNum();
}

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  return value;
}

void performScan() {
  const int count = WiFi.scanNetworks(false, true);
  String json = "[";
  for (int i = 0; i < count; ++i) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) +
            "\",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"channel\":" + String(WiFi.channel(i)) +
            ",\"secure\":" +
            String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") +
            "}";
  }
  json += "]";
  scanJson = json;
  WiFi.scanDelete();
  lastScanMs = millis();
}

void maintainUpstream() {
  if (WiFi.status() == WL_CONNECTED) {
    reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
    return;
  }

  if (targetSsid.length() == 0) return;
  if (millis() - lastReconnectAttempt < reconnectDelayMs) return;

  lastReconnectAttempt = millis();
  WiFi.begin(targetSsid.c_str(), targetPassword.c_str());
  reconnectDelayMs = (reconnectDelayMs * 2UL > static_cast<unsigned long>(RangeLinkConfig::RECONNECT_MAX_MS)) ? static_cast<unsigned long>(RangeLinkConfig::RECONNECT_MAX_MS) : reconnectDelayMs * 2UL;
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
  refreshState();
}

void wifiManagerLoop() {
  maintainUpstream();

  if (!state.upstreamConnected &&
      millis() - lastScanMs >= RangeLinkConfig::SCAN_INTERVAL_MS) {
    performScan();
  }

  refreshState();
}

void requestWifiScan() {
  performScan();
}

String getWifiScanJson() {
  return scanJson;
}

SystemState getSystemState() {
  refreshState();
  return state;
}

bool connectUpstream(const String& ssid, const String& password) {
  if (ssid.length() == 0 || password.length() < 8) return false;

  targetSsid = ssid;
  targetPassword = password;

  reconnectDelayMs = RangeLinkConfig::RECONNECT_MIN_MS;
  lastReconnectAttempt = 0;

  WiFi.disconnect();
  WiFi.begin(targetSsid.c_str(), targetPassword.c_str());
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
