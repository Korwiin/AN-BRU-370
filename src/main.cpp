#include <Arduino.h>
#include <pins.h>
#include <config.h>

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== Brew370 v%s boot ===\n", FIRMWARE_VERSION);
}

void loop() {
}
