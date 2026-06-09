#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>

// DCS-BIOS F-16C verified identifiers (F-16C_50.json v0.11.4)
// AP switches + MC button + STORES_CONFIG_SW share address 0x4400.
// RWR MSL LAUNCH at 0x4480 (F_16C_50_LIGHT_RWR_MSL_LAUNCH, Addresses.h line 15956).
// ANT ELEV not modeled in DCS-BIOS — handled via USB HID Gamepad axis.

constexpr uint16_t DCSBIOS_ADDR_AP_SWITCHES = 0x4400;
constexpr uint16_t DCSBIOS_MASK_AP_PITCH    = 0x0300;
constexpr uint8_t  DCSBIOS_SHFT_AP_PITCH    = 8;
constexpr uint16_t DCSBIOS_MASK_AP_ROLL     = 0x0C00;
constexpr uint8_t  DCSBIOS_SHFT_AP_ROLL     = 10;

constexpr uint16_t DCSBIOS_ADDR_MC_LIGHT    = 0x447A;
constexpr uint16_t DCSBIOS_MASK_MC_LIGHT    = 0x0001;

constexpr uint16_t DCSBIOS_ADDR_RWR_MSL_LAUNCH  = 0x4480;
constexpr uint16_t DCSBIOS_MASK_RWR_MSL_LAUNCH  = 0x0004;

constexpr uint16_t DCSBIOS_ADDR_STORES_CONFIG_LIGHT = 0x4478;
constexpr uint16_t DCSBIOS_MASK_STORES_CONFIG_LIGHT = 0x0001;

// STORES_CONFIG_SW shares DCSBIOS_ADDR_AP_SWITCHES (0x4400)
constexpr uint16_t DCSBIOS_MASK_STORES_CONFIG_SW = 0x0080;
constexpr uint8_t  DCSBIOS_SHFT_STORES_CONFIG_SW = 7;

#define DCSBIOS_CMD_MC_RESET  "MASTER_CAUTION"
#define DCSBIOS_CMD_CMDS_DISPENSE  "CMDS_DISPENSE_BTN"
#define DCSBIOS_CMD_STORES_CONFIG_SW  "STORES_CONFIG_SW"

// Smart-send timing (ms). Tune SC_LIGHT_TIMEOUT_MS based on observed LT values.
constexpr uint32_t SC_RETRY_MS          = 500;
constexpr uint32_t SC_LIGHT_TIMEOUT_MS  = 3000;

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
  bool    rwrMslLaunch();   // true = RWR MISSILE LAUNCH light illuminated
  bool    storesConfigLight(); // true = STORES CONFIG light illuminated
  uint8_t storesConfigSw();    // 0=CAT I, 1=CAT III, 0xFF=not yet received
}
