#pragma once
#include <Arduino.h>
#include "models.h"

void wifiManagerBegin();
void wifiManagerLoop();

void requestWifiScan();
String getWifiScanJson();
String getSavedProfilesJson();
String getChannelAnalysisJson();
SystemState getSystemState();

bool connectUpstream(
  const String& ssid,
  const String& password,
  bool openNetwork = false
);
bool connectSavedProfile(const String& ssid);
bool forgetSavedProfile(const String& ssid);
void reconnectUpstream();

bool setAccessPointCredentials(const String& ssid, const String& password);
