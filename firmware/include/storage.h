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

void factoryResetStorage();
