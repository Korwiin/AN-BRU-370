#pragma once
#include <cstdint>

// --- Pin map — Brew370 / ESP32-S3 Super Mini (DWEII) ---
// All front-side pins. GPIO 1, 2, 12, 13 freed (formerly AP switches).
// See Board Diagrams/ESP32-S3_DWEII_Pinout.jpg

// OLED I2C (SSD1306 128x32)
constexpr uint8_t PIN_OLED_SDA   = 5;
constexpr uint8_t PIN_OLED_SCL   = 6;

// Rotary encoder (EC11)
constexpr uint8_t PIN_ENC_CLK    = 3;
constexpr uint8_t PIN_ENC_DT     = 4;
constexpr uint8_t PIN_ENC_SW     = 10;  // INPUT_PULLUP, active low

