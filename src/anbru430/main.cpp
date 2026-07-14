#include <Arduino.h>
#include <USB.h>
#include "config.h"
#include "pins.h"

void setup() {
  // Same identity pattern as core hid.cpp (proven with CDC_ON_BOOT=1).
  USB.manufacturerName("E4 Mafia");
  USB.productName(USB_PRODUCT_NAME);
  USB.PID(USB_PID);
  USB.firmwareVersion(FIRMWARE_VERSION_INT);
  USB.begin();

  Serial.begin(115200);
  while (!Serial && millis() < 2000) { }
  Serial.println("=== ANBRU-430 boot ===");
  Serial.printf("fw v%s, PSRAM: %u bytes free\n", FIRMWARE_VERSION, ESP.getFreePsram());
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last >= 1000) {
    last += 1000;
    Serial.printf("alive %lus, heap %u, psram %u\n",
                  millis() / 1000, ESP.getFreeHeap(), ESP.getFreePsram());
  }
}
