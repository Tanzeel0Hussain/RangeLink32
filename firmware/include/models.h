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
  uint64_t rxBytes = 0;
  uint64_t txBytes = 0;
  bool approved = false;
  bool blocked = false;
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
