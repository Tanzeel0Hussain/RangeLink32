#pragma once
#include <Arduino.h>

void storageBegin();

String getApSsid();
String getApPassword();
String getAdminUser();
String getAdminPassword();

bool setApCredentials(const String& ssid, const String& password);
bool setAdminCredentials(const String& username, const String& password);
void factoryResetStorage();
