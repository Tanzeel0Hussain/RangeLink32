#pragma once
#include <Arduino.h>

struct WifiProfile {
  String ssid;
  String secret;
  int priority = 100;
  int32_t lastRssi = -127;
  uint64_t rxBytes = 0;
  uint64_t txBytes = 0;
  bool enabled = true;
};

struct ClientRecord {
  String hostname;
  String ip;
  String mac;
  int32_t rssi = -127;

  uint64_t rxBytes = 0;
  uint64_t txBytes = 0;
  uint64_t dailyRxBytes = 0;
  uint64_t dailyTxBytes = 0;
  uint64_t dailyQuotaBytes = 0;

  uint32_t bandwidthKbps = 0;
  uint32_t guestUntilEpoch = 0;
  int32_t usageDay = -1;

  uint8_t scheduleStartHour = 0;
  uint8_t scheduleEndHour = 24;

  bool connected = false;
  bool approved = false;
  bool blocked = false;
  bool scheduleEnabled = false;
};

struct SystemState {
  bool upstreamConnected = false;
  bool internetReachable = false;
  String upstreamSsid;
  int32_t upstreamRssi = -127;
  String apSsid;
  String apIp = "192.168.50.1";
  uint32_t connectedClients = 0;
};
