#pragma once
#include <Arduino.h>

namespace RangeLinkConfig {
constexpr char DEVICE_NAME[] = "RangeLink32";
constexpr char DEFAULT_AP_SSID[] = "RangeLink32-Setup";
constexpr char DEFAULT_AP_PASSWORD[] = "rangelink32";
constexpr char DEFAULT_ADMIN_USER[] = "admin";
constexpr char DEFAULT_ADMIN_PASSWORD[] = "changeme32";

constexpr uint8_t AP_IP_A = 192;
constexpr uint8_t AP_IP_B = 168;
constexpr uint8_t AP_IP_C = 50;
constexpr uint8_t AP_IP_D = 1;

constexpr uint32_t RECONNECT_MIN_MS = 2000;
constexpr uint32_t RECONNECT_MAX_MS = 30000;
constexpr uint32_t SCAN_INTERVAL_MS = 30000;
constexpr uint8_t MAX_WIFI_PROFILES = 8;
constexpr uint8_t MAX_CLIENT_RECORDS = 24;
}
