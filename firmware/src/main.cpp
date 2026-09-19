#include <Arduino.h>
#include "storage.h"
#include "wifi_manager.h"
#include "traffic_monitor.h"
#include "access_control.h"
#include "router_engine.h"
#include "web_admin.h"
#include "system_monitor.h"

void setup() {
  Serial.begin(115200);
  delay(250);

  storageBegin();
  wifiManagerBegin();
  trafficMonitorBegin();
  accessControlBegin();
  routerEngineBegin();
  webAdminBegin();
  systemMonitorBegin();

  Serial.println();
  Serial.println("RangeLink32 started");
  Serial.println("Management: http://192.168.50.1");
}

void loop() {
  wifiManagerLoop();
  trafficMonitorLoop();
  accessControlLoop();
  routerEngineLoop();
  webAdminLoop();
  systemMonitorLoop();

  delay(2);
}
