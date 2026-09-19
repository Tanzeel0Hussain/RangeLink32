#include <Arduino.h>
#include "storage.h"
#include "wifi_manager.h"
#include "access_control.h"
#include "router_engine.h"
#include "web_admin.h"

void setup() {
  Serial.begin(115200);
  delay(250);

  storageBegin();
  wifiManagerBegin();
  accessControlBegin();
  routerEngineBegin();
  webAdminBegin();

  Serial.println();
  Serial.println("RangeLink32 started");
  Serial.println("Management: http://192.168.50.1");
}

void loop() {
  wifiManagerLoop();
  accessControlLoop();
  routerEngineLoop();
  webAdminLoop();
  delay(2);
}
