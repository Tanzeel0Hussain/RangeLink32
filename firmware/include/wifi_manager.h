#pragma once
#include <Arduino.h>
#include "models.h"

void wifiManagerBegin();
void wifiManagerLoop();

void requestWifiScan();
String getWifiScanJson();
SystemState getSystemState();

bool connectUpstream(const String& ssid, const String& password);
void reconnectUpstream();
bool setAccessPointCredentials(const String& ssid, const String& password);
