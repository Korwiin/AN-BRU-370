#pragma once
#include <cstdint>

// --- Pin map — Brew370 ---
// See Board Diagrams/ESP32-S3_DWEII_Pinout.jpg

// DEV_PINS: breadboard wiring without the rest of DEV_BUILD (HID stays active) —
// lets production USB behavior be bench-tested on the dev board.
#if defined(DEV_BUILD) || defined(DEV_PINS)
// Dev breadboard
constexpr uint8_t PIN_OLED_SDA   = 5;
constexpr uint8_t PIN_OLED_SCL   = 6;
constexpr uint8_t PIN_ENC_CLK    = 3;
constexpr uint8_t PIN_ENC_DT     = 4;
constexpr uint8_t PIN_ENC_SW     = 10;  // INPUT_PULLUP, active low
#else
// Prod enclosure (ESP32-S3 DWEII Super Mini)
constexpr uint8_t PIN_OLED_SDA   = 11;
constexpr uint8_t PIN_OLED_SCL   = 12;
constexpr uint8_t PIN_ENC_CLK    = 5;
constexpr uint8_t PIN_ENC_DT     = 3;
constexpr uint8_t PIN_ENC_SW     = 4;   // INPUT_PULLUP, active low
#endif
