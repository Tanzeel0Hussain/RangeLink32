#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

extern "C" {
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"
}

#include "access_control.h"
#include "storage.h"
#include "config.h"

namespace {
constexpr unsigned long CLIENT_REFRESH_MS = 2000;

AccessMode mode = AccessMode::AllowAll;
ClientRecord liveClients[RangeLinkConfig::MAX_CLIENT_RECORDS];
size_t liveCount = 0;

ClientRecord policies[RangeLinkConfig::MAX_CLIENT_RECORDS];
size_t policyCount = 0;

unsigned long lastRefreshMs = 0;

String macToString(const uint8_t mac[6]) {
  char out[18];
  snprintf(
    out,
    sizeof(out),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
  );
  return String(out);
}

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  return value;
}

int policyIndex(const String& mac) {
  for (size_t i = 0; i < policyCount; ++i) {
    if (policies[i].mac.equalsIgnoreCase(mac)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void reloadPolicies() {
  policyCount =
    loadClientPolicies(policies, RangeLinkConfig::MAX_CLIENT_RECORDS);
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

void disconnectMac(const uint8_t mac[6]) {
  uint16_t aid = 0;
  if (esp_wifi_ap_get_sta_aid(mac, &aid) == ESP_OK && aid != 0) {
    esp_wifi_deauth_sta(aid);
  }
}

bool policyAllows(const String& mac) {
  const int index = policyIndex(mac);

  if (index >= 0 && policies[index].blocked) {
    return false;
  }

  if (mode == AccessMode::AllowAll) {
    return true;
  }

  return index >= 0 && policies[index].approved;
}

void refreshClients(bool enforcePolicy) {
  wifi_sta_list_t wifiList = {};
  if (esp_wifi_ap_get_sta_list(&wifiList) != ESP_OK) {
    liveCount = 0;
    lastRefreshMs = millis();
    return;
  }

  wifi_sta_mac_ip_list_t ipList = {};
  const bool haveIpList =
    esp_wifi_ap_get_sta_list_with_ip(&wifiList, &ipList) == ESP_OK;

  liveCount = 0;
  const size_t count =
    static_cast<size_t>(wifiList.num) > RangeLinkConfig::MAX_CLIENT_RECORDS
      ? RangeLinkConfig::MAX_CLIENT_RECORDS
      : static_cast<size_t>(wifiList.num);

  for (size_t i = 0; i < count; ++i) {
    ClientRecord record;
    record.mac = macToString(wifiList.sta[i].mac);
    record.rssi = wifiList.sta[i].rssi;
    record.connected = true;
    record.ip = "0.0.0.0";

    if (haveIpList) {
      ipForMac(ipList, wifiList.sta[i].mac, record.ip);
    }

    int index = policyIndex(record.mac);
    if (index < 0) {
      ClientRecord seen;
      seen.mac = record.mac;
      saveClientPolicy(seen);
      reloadPolicies();
      index = policyIndex(record.mac);
    }

    if (index >= 0) {
      record.approved = policies[index].approved;
      record.blocked = policies[index].blocked;
    }

    liveClients[liveCount++] = record;

    if (enforcePolicy && !policyAllows(record.mac)) {
      disconnectMac(wifiList.sta[i].mac);
    }
  }

  lastRefreshMs = millis();
}

ClientRecord policyFor(const String& mac) {
  const int index = policyIndex(mac);
  if (index >= 0) {
    return policies[index];
  }

  ClientRecord record;
  record.mac = mac;
  return record;
}
}

void accessControlBegin() {
  const uint8_t stored = getStoredAccessMode();
  mode = stored == static_cast<uint8_t>(AccessMode::AllowlistOnly)
    ? AccessMode::AllowlistOnly
    : AccessMode::AllowAll;

  reloadPolicies();
  refreshClients(true);
}

void accessControlLoop() {
  if (millis() - lastRefreshMs >= CLIENT_REFRESH_MS) {
    refreshClients(true);
  }
}

AccessMode getAccessMode() {
  return mode;
}

void setAccessMode(AccessMode newMode) {
  mode = newMode;
  setStoredAccessMode(static_cast<uint8_t>(newMode));
  refreshClients(true);
}

bool setClientApproval(const String& mac, bool approved) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);
  record.mac = mac;
  record.approved = approved;

  if (!saveClientPolicy(record)) return false;

  reloadPolicies();
  refreshClients(true);
  return true;
}

bool setClientBlocked(const String& mac, bool blocked) {
  if (mac.length() != 17) return false;

  ClientRecord record = policyFor(mac);
  record.mac = mac;
  record.blocked = blocked;

  if (!saveClientPolicy(record)) return false;

  reloadPolicies();
  refreshClients(true);
  return true;
}

bool clientMayUseInternet(const String& mac) {
  return policyAllows(mac);
}

String getClientTableJson() {
  if (millis() - lastRefreshMs >= 750) {
    refreshClients(false);
  }

  String json = "[";

  for (size_t p = 0; p < policyCount; ++p) {
    if (p) json += ",";

    ClientRecord record = policies[p];

    for (size_t i = 0; i < liveCount; ++i) {
      if (liveClients[i].mac.equalsIgnoreCase(record.mac)) {
        record.ip = liveClients[i].ip;
        record.rssi = liveClients[i].rssi;
        record.connected = true;
        break;
      }
    }

    json += "{\"hostname\":\"" + jsonEscape(record.hostname) +
            "\",\"ip\":\"" + jsonEscape(record.ip) +
            "\",\"mac\":\"" + jsonEscape(record.mac) +
            "\",\"rssi\":" + String(record.rssi) +
            ",\"connected\":" + String(record.connected ? "true" : "false") +
            ",\"approved\":" + String(record.approved ? "true" : "false") +
            ",\"blocked\":" + String(record.blocked ? "true" : "false") +
            ",\"allowed\":" +
            String(clientMayUseInternet(record.mac) ? "true" : "false") +
            ",\"rxBytes\":" + String(static_cast<unsigned long long>(record.rxBytes)) +
            ",\"txBytes\":" + String(static_cast<unsigned long long>(record.txBytes)) +
            "}";
  }

  json += "]";
  return json;
}
