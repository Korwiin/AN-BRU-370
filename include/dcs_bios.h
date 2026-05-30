#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>

// DCS-BIOS F-16C verified identifiers (F-16C_50.json v0.11.4)
// AP switches + MC button share address 0x4400.
// ANT ELEV not modeled in DCS-BIOS — handled via USB HID Gamepad axis.

constexpr uint16_t DCSBIOS_ADDR_AP_SWITCHES = 0x4400;
constexpr uint16_t DCSBIOS_MASK_AP_PITCH    = 0x0300;
constexpr uint8_t  DCSBIOS_SHFT_AP_PITCH    = 8;
constexpr uint16_t DCSBIOS_MASK_AP_ROLL     = 0x0C00;
constexpr uint8_t  DCSBIOS_SHFT_AP_ROLL     = 10;

constexpr uint16_t DCSBIOS_ADDR_MC_LIGHT    = 0x447A;
constexpr uint16_t DCSBIOS_MASK_MC_LIGHT    = 0x0001;

#define DCSBIOS_CMD_MC_RESET  "MASTER_CAUTION"

namespace DcsBios {
  // Call after WiFi is connected.
  void begin(const char* mcastAddr, uint16_t listenPort,
             const char* cmdHost,   uint16_t cmdPort);

  // Call every loop(). Processes incoming UDP frames.
  // Returns true if at least one byte was received this call.
  bool update();

  // True if valid frames received within last 3 seconds.
  bool isConnected();

  // Send plain-text command "IDENTIFIER VALUE\n" — use only for MC reset.
  void sendCommand(const char* identifier, uint16_t value);

  // Latest decoded values (0xFF = not yet received from stream)
  uint8_t apPitchSwitch();  // 0=ATT HOLD / 1=A/P OFF / 2=ALT HOLD
  uint8_t apRollSwitch();   // 0=STRG SEL / 1=ATT HOLD / 2=HDG SEL
  bool    masterCaution();  // true = MASTER CAUTION light illuminated
}
