#pragma once
#include <cstdint>

// --- Pin map — Brew370 / ESP32-S3 Super Mini (DWEII) ---
// All front-side pins. ADC1 only (Wi-Fi safe) reserved for future use.
// See Board Diagrams/ESP32-S3_DWEII_Pinout.jpg

// OLED I2C (SSD1306 128x32)
constexpr uint8_t PIN_OLED_SDA   = 5;
constexpr uint8_t PIN_OLED_SCL   = 6;

// Rotary encoder (EC11)
constexpr uint8_t PIN_ENC_CLK    = 8;
constexpr uint8_t PIN_ENC_DT     = 9;
constexpr uint8_t PIN_ENC_SW     = 10;  // INPUT_PULLUP, active low

// AP PITCH switch (3-pos, ATT HOLD / A/P OFF / ALT HOLD)
// COM -> GND; throws -> GPIO with INPUT_PULLUP
// {LOW,HIGH}=pos0  {HIGH,HIGH}=pos1(center/off)  {HIGH,LOW}=pos2
constexpr uint8_t PIN_SW1_A      = 1;   // INPUT_PULLUP
constexpr uint8_t PIN_SW1_B      = 2;   // INPUT_PULLUP

// AP ROLL switch (3-pos, STRG SEL / ATT HOLD / HDG SEL)
constexpr uint8_t PIN_SW2_A      = 3;   // INPUT_PULLUP
constexpr uint8_t PIN_SW2_B      = 4;   // INPUT_PULLUP

