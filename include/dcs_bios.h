#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>

// DCS-BIOS F-16C verified identifiers (F-16C_50.json v0.11.4)
// RWR MSL LAUNCH at 0x4480 (F_16C_50_LIGHT_RWR_MSL_LAUNCH, Addresses.h line 15956).

constexpr uint16_t DCSBIOS_ADDR_MC_LIGHT    = 0x447A;
constexpr uint16_t DCSBIOS_MASK_MC_LIGHT    = 0x0001;

constexpr uint16_t DCSBIOS_ADDR_RWR_MSL_LAUNCH  = 0x4480;
constexpr uint16_t DCSBIOS_MASK_RWR_MSL_LAUNCH  = 0x0004;

constexpr uint16_t DCSBIOS_ADDR_STORES_CONFIG_LIGHT = 0x4478;
constexpr uint16_t DCSBIOS_MASK_STORES_CONFIG_LIGHT = 0x0001;

// STORES_CONFIG_SW at 0x4400 (previously shared with AP switches)
constexpr uint16_t DCSBIOS_ADDR_STORES_CONFIG_SW = 0x4400;
constexpr uint16_t DCSBIOS_MASK_STORES_CONFIG_SW = 0x0080;
constexpr uint8_t  DCSBIOS_SHFT_STORES_CONFIG_SW = 7;

// Fuel totalizer dials — each uint16 maps dial position 0–9 to 0–65535
constexpr uint16_t DCSBIOS_ADDR_FUEL_10K  = 0x44EE;
constexpr uint16_t DCSBIOS_ADDR_FUEL_1K   = 0x44F0;
constexpr uint16_t DCSBIOS_ADDR_FUEL_100  = 0x44F2;

// CMDS chaff/flare amount strings — 4 ASCII chars each, 2 words each
constexpr uint16_t DCSBIOS_ADDR_CH_AMT_0  = 0x45A8;  // chars [0..1]
constexpr uint16_t DCSBIOS_ADDR_CH_AMT_1  = 0x45AA;  // chars [2..3]; parse on arrival
constexpr uint16_t DCSBIOS_ADDR_FL_AMT_0  = 0x45AC;  // chars [0..1]
constexpr uint16_t DCSBIOS_ADDR_FL_AMT_1  = 0x45AE;  // chars [2..3]; parse on arrival

// ECM transmit light (green) — 1 when jammer pod is actively transmitting
constexpr uint16_t DCSBIOS_ADDR_ECM_TX    = 0x4544;
constexpr uint16_t DCSBIOS_MASK_ECM_TX    = 0x4000;

#define DCSBIOS_CMD_MC_RESET           "MASTER_CAUTION"
#define DCSBIOS_CMD_CMDS_DISPENSE      "CMDS_DISPENSE_BTN"
#define DCSBIOS_CMD_STORES_CONFIG_SW   "STORES_CONFIG_SW"

constexpr uint32_t SC_RETRY_MS         = 500;
constexpr uint32_t SC_LIGHT_TIMEOUT_MS = 3000;

namespace DcsBios {
  void begin(const char* mcastAddr, uint16_t listenPort,
             const char* cmdHost,   uint16_t cmdPort);
  bool update();
  bool isConnected();
  bool hasData();   // true while DCS-BIOS packets received (same 3s window as isConnected)
  void sendCommand(const char* identifier, uint16_t value);

  bool    masterCaution();
  bool    rwrMslLaunch();
  bool    storesConfigLight();
  uint8_t storesConfigSw();
  uint32_t fuelLbs();
  uint8_t  chaffCount();
  uint8_t  flareCount();
  bool     ecmTransmitting();
}
