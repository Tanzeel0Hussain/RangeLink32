#pragma once
#include <Arduino.h>
#include "models.h"

void trafficMonitorBegin();
void trafficMonitorLoop();

void trafficMonitorSetDefaultAllow(bool allowed);
void trafficMonitorConfigureClient(
  const ClientRecord& record,
  bool internetAllowed
);

bool trafficMonitorGetStats(
  const String& mac,
  uint64_t& rxBytes,
  uint64_t& txBytes,
  uint64_t& dailyRxBytes,
  uint64_t& dailyTxBytes
);

bool trafficMonitorResetUsage(const String& mac, bool resetTotal);
void trafficMonitorGetTotals(
  uint64_t& rxBytes,
  uint64_t& txBytes
);
