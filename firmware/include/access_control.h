#pragma once
#include <Arduino.h>
#include "models.h"

enum class AccessMode : uint8_t {
  AllowAll = 0,
  AllowlistOnly = 1
};

void accessControlBegin();
void accessControlLoop();
void accessControlFlush();
bool accessControlTimeSynchronized();

AccessMode getAccessMode();
void setAccessMode(AccessMode mode);

bool setClientApproval(const String& mac, bool approved);
bool setClientBlocked(const String& mac, bool blocked);
bool setClientName(const String& mac, const String& name);
bool setClientLimits(
  const String& mac,
  uint64_t dailyQuotaBytes,
  uint64_t monthlyQuotaBytes,
  uint32_t bandwidthKbps
);
bool setClientSchedule(
  const String& mac,
  bool enabled,
  uint8_t startHour,
  uint8_t endHour
);
bool grantGuestAccess(const String& mac, uint32_t minutes);
bool resetClientUsage(const String& mac, bool resetTotal);
bool resetClientMonthlyUsage(const String& mac);
bool forgetKnownClient(const String& mac);

bool clientMayUseInternet(const String& mac);
String getClientTableJson();
