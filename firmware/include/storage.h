#pragma once
#include <Arduino.h>
#include "models.h"

void storageBegin();

String getApSsid();
String getApPassword();
String getAdminUser();
String getAdminPassword();
bool credentialRecoveryRequired();

bool setApCredentials(const String& ssid, const String& password);
bool setAdminCredentials(const String& username, const String& password);
bool initialSetupRequired();
bool setInitialCredentials(
  const String& ssid,
  const String& apPassword,
  const String& adminUser,
  const String& adminPassword
);

size_t loadWifiProfiles(WifiProfile* out, size_t maxCount);
bool saveWifiProfile(const WifiProfile& profile);
bool removeWifiProfile(const String& ssid);
bool setWifiProfilePriority(
  const String& ssid,
  int priority
);
bool getWifiProfileSecret(const String& ssid, String& secret);
bool updateWifiProfileUsage(
  const String& ssid,
  int32_t rssi,
  uint64_t rxDelta,
  uint64_t txDelta
);

uint8_t getStoredAccessMode();
bool setStoredAccessMode(uint8_t mode);

size_t loadClientPolicies(ClientRecord* out, size_t maxCount);
bool saveClientPolicy(const ClientRecord& record);
bool removeClientPolicy(const String& mac);

int getTimezoneOffsetMinutes();
bool setTimezoneOffsetMinutes(int minutes);

String getCustomDns();
bool setCustomDns(const String& dns);

void appendEventLog(const String& type, const String& message);
String getEventLogJson();
void clearEventLogs();

String exportSafeSettings();
bool importSafeSettings(const String& text);

void factoryResetStorage();
