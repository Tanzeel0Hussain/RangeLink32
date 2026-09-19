#pragma once
#include <Arduino.h>
#include "models.h"

void storageBegin();

String getApSsid();
String getApPassword();
String getAdminUser();
String getAdminPassword();

bool setApCredentials(const String& ssid, const String& password);
bool setAdminCredentials(const String& username, const String& password);

size_t loadWifiProfiles(WifiProfile* out, size_t maxCount);
bool saveWifiProfile(const WifiProfile& profile);
bool removeWifiProfile(const String& ssid);

uint8_t getStoredAccessMode();
void setStoredAccessMode(uint8_t mode);

size_t loadClientPolicies(ClientRecord* out, size_t maxCount);
bool saveClientPolicy(const ClientRecord& record);

int getTimezoneOffsetMinutes();
void setTimezoneOffsetMinutes(int minutes);

void appendEventLog(const String& type, const String& message);
String getEventLogJson();
void clearEventLogs();

void factoryResetStorage();
