#pragma once
#include <Arduino.h>
#include "models.h"

enum class AccessMode : uint8_t {
  AllowAll = 0,
  AllowlistOnly = 1
};

void accessControlBegin();
void accessControlLoop();

AccessMode getAccessMode();
void setAccessMode(AccessMode mode);

bool setClientApproval(const String& mac, bool approved);
bool setClientBlocked(const String& mac, bool blocked);
bool clientMayUseInternet(const String& mac);
String getClientTableJson();
