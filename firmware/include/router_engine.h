#pragma once
#include <Arduino.h>

void routerEngineBegin();
void routerEngineLoop();

bool routerEngineReady();
bool setClientInternetAccess(const String& mac, bool allowed);
