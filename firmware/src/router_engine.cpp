#include <WiFi.h>
#include "router_engine.h"

namespace {
bool ready = false;

void setNapt(bool enabled) {
  const bool ok = WiFi.AP.enableNAPT(enabled);
  ready = enabled && ok;

  Serial.print("RangeLink32 NAPT: ");
  if (!enabled) {
    Serial.println("disabled");
  } else {
    Serial.println(ok ? "enabled" : "enable failed");
  }
}
}

void routerEngineBegin() {
  ready = false;
  WiFi.AP.enableNAPT(false);
}

void routerEngineLoop() {
  const bool shouldRoute = WiFi.status() == WL_CONNECTED;

  if (shouldRoute && !ready) {
    setNapt(true);
  } else if (!shouldRoute && ready) {
    setNapt(false);
  }
}

bool routerEngineReady() {
  return ready;
}

