#include <WiFi.h>

extern "C" {
#include "lwip/lwip_napt.h"
}

#include "router_engine.h"

namespace {
bool ready = false;

void setNapt(bool enabled) {
  const uint32_t apAddress = static_cast<uint32_t>(WiFi.softAPIP());
  ip_napt_enable(apAddress, enabled ? 1 : 0);
  ready = enabled;

  Serial.print("RangeLink32 NAPT: ");
  Serial.println(enabled ? "enabled" : "disabled");
}
}

void routerEngineBegin() {
  ready = false;
  setNapt(false);
}

void routerEngineLoop() {
  const bool shouldRoute = WiFi.status() == WL_CONNECTED;

  if (shouldRoute != ready) {
    setNapt(shouldRoute);
  }
}

bool routerEngineReady() {
  return ready;
}

bool setClientInternetAccess(const String&, bool) {
  // Per-client forwarding policy is implemented in the access-control
  // milestone. NAPT itself is currently enabled/disabled for the AP.
  return false;
}
