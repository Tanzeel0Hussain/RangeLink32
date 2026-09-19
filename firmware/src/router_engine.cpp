#include "router_engine.h"

namespace {
bool ready = false;
}

void routerEngineBegin() {
  /*
   * NAT/NAPT implementation belongs only in this module.
   * This keeps ESP32/lwIP-specific forwarding code away from the dashboard,
   * Wi-Fi profile manager and access-control modules.
   *
   * The forwarding engine will be enabled after CI compilation confirms the
   * selected Arduino-ESP32/lwIP API and then verified on the real ESP32U.
   */
  ready = false;
}

void routerEngineLoop() {}

bool routerEngineReady() {
  return ready;
}

bool setClientInternetAccess(const String&, bool) {
  return false;
}
