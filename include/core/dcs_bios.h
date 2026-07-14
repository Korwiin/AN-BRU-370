#pragma once
#include <Arduino.h>

// DCS-BIOS F-16C verified identifiers (F-16C_50.json v0.11.4)
// RWR MSL LAUNCH at 0x4480 (F_16C_50_LIGHT_RWR_MSL_LAUNCH, Addresses.h line 15956).

constexpr uint16_t DCSBIOS_ADDR_MC_LIGHT    = 0x447A;
constexpr uint16_t DCSBIOS_MASK_MC_LIGHT    = 0x0001;

constexpr uint16_t DCSBIOS_ADDR_RWR_MSL_LAUNCH  = 0x4480;
constexpr uint16_t DCSBIOS_MASK_RWR_MSL_LAUNCH  = 0x0004;
// ECM_1_BTN S-light — shares address 0x4480; lit = pod installed in STBY
constexpr uint16_t DCSBIOS_MASK_ECM_1_S         = 0x2000;

// ECM button S-lights 2–5 — all at 0x448A; all must be lit to confirm step 6
constexpr uint16_t DCSBIOS_ADDR_ECM_BTNS        = 0x448A;
constexpr uint16_t DCSBIOS_MASK_ECM_2_S         = 0x0002;
constexpr uint16_t DCSBIOS_MASK_ECM_3_S         = 0x0020;
constexpr uint16_t DCSBIOS_MASK_ECM_4_S         = 0x0200;
constexpr uint16_t DCSBIOS_MASK_ECM_5_S         = 0x2000;

// ECM power switch readback — 0x4526; value 2 = OPR
// ECM XMIT switch — shares 0x4526; value 2 = AFT
constexpr uint16_t DCSBIOS_ADDR_ECM_PW_SW       = 0x4526;
constexpr uint16_t DCSBIOS_MASK_ECM_PW_SW       = 0x0300;
constexpr uint8_t  DCSBIOS_SHFT_ECM_PW_SW       = 8;
constexpr uint16_t DCSBIOS_MASK_ECM_XMIT_SW     = 0x0C00;
constexpr uint8_t  DCSBIOS_SHFT_ECM_XMIT_SW     = 10;

// CMDS JMR Source switch — shares address 0x445A with MWS_SW; value 1 = ON
constexpr uint16_t DCSBIOS_MASK_JMR_SW          = 0x0800;

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

// CMDS chaff amount string — 4 ASCII chars, 2 words
constexpr uint16_t DCSBIOS_ADDR_CH_AMT_0  = 0x45A8;  // chars [0..1]
constexpr uint16_t DCSBIOS_ADDR_CH_AMT_1  = 0x45AA;  // chars [2..3]; parse on arrival


// New-plane setup — MWS source switch used as configuration flag
constexpr uint16_t DCSBIOS_ADDR_MWS_SW        = 0x445A;
constexpr uint16_t DCSBIOS_MASK_MWS_SW        = 0x1000;

// Sensor panel hardpoints — HAD=Left, TGP=Right (share address 0x4424)
constexpr uint16_t DCSBIOS_ADDR_HDPT          = 0x4424;
constexpr uint16_t DCSBIOS_MASK_HDPT_L        = 0x2000;
constexpr uint16_t DCSBIOS_MASK_HDPT_R        = 0x4000;

// CMDS MODE knob — SEMI is position 3 (OFF=0 STBY=1 MAN=2 SEMI=3 AUTO=4 BYP=5)
constexpr uint16_t DCSBIOS_ADDR_CMDS_MODE     = 0x445E;
constexpr uint16_t DCSBIOS_MASK_CMDS_MODE     = 0x00E0;
constexpr uint8_t  DCSBIOS_SHFT_CMDS_MODE     = 5;

// RWR POWER light — green indicator, used to verify RWR setup step
constexpr uint16_t DCSBIOS_ADDR_RWR_PWR_LIGHT = 0x447E;
constexpr uint16_t DCSBIOS_MASK_RWR_PWR_LIGHT = 0x8000;

#define DCSBIOS_CMD_ECM_2_BTN          "ECM_2_BTN"
#define DCSBIOS_CMD_ECM_3_BTN          "ECM_3_BTN"
#define DCSBIOS_CMD_ECM_4_BTN          "ECM_4_BTN"
#define DCSBIOS_CMD_ECM_5_BTN          "ECM_5_BTN"
#define DCSBIOS_CMD_ECM_6_BTN          "ECM_6_BTN"
#define DCSBIOS_CMD_ECM_PW_SW          "ECM_PW_SW"
#define DCSBIOS_CMD_ECM_XMIT_SW        "ECM_XMIT_SW"
#define DCSBIOS_CMD_JMR_SW             "CMDS_JMR_SOURCHE_SW"

#define DCSBIOS_CMD_MC_RESET           "MASTER_CAUTION"
#define DCSBIOS_CMD_CMDS_DISPENSE      "CMDS_DISPENSE_BTN"
#define DCSBIOS_CMD_STORES_CONFIG_SW   "STORES_CONFIG_SW"
#define DCSBIOS_CMD_HDPT_SW_L          "HDPT_SW_L"
#define DCSBIOS_CMD_HDPT_SW_R          "HDPT_SW_R"
#define DCSBIOS_CMD_CMDS_MODE_KNB      "CMDS_MODE_KNB"
#define DCSBIOS_CMD_RWR_PWR_BTN        "RWR_PWR_BTN"
#define DCSBIOS_CMD_MWS_SW         "CMDS_MWS_SOURCHE_SW"

constexpr uint32_t SC_RETRY_MS         = 500;
constexpr uint32_t SC_LIGHT_TIMEOUT_MS = 3000;
constexpr uint32_t SETUP_RETRY_MS    = 750;
constexpr uint8_t  SETUP_MAX_RETRIES = 10;   // 10 × 500 ms = 5 s max per step before skip

namespace DcsBios {
  void begin(const char* mcastAddr, uint16_t listenPort,
             const char* cmdHost,   uint16_t cmdPort);  // cmdHost ignored; sender IP learned from first packet
  bool process();   // drain ring buffer and parse; call from Core 1 / loop() only
  bool isConnected();
  void sendCommand(const char* identifier, uint16_t value);

  bool    masterCaution();
  bool    rwrMslLaunch();
  bool    storesConfigLight();
  uint8_t storesConfigSw();
  uint32_t    fuelLbs();
  const char* chaffStr();   // raw 4-char DCS string e.g. "  60", "Lo10"; "    " = not received

  bool    mwsOn();           // true when CMDS_MWS_SOURCHE_SW reads 1
  bool    hdptLeft();        // true when left hardpoint (HAD) switch is ON
  bool    hdptRight();       // true when right hardpoint (TGP) switch is ON
  uint8_t cmdsModeKnob();    // CMDS MODE knob position: SEMI=3
  bool    rwrPowerLight();   // true when RWR POWER light is lit

  // ECM setup — ALQ-131 pod detection and configuration
  bool    ecmStandby();       // LIGHT_ECM_1_S lit: pod installed, power in STBY
  bool    ecmBtns2to5Armed(); // S-lights on buttons 2-5 all lit (step 6 confirm)
  bool    ecmPowerOpr();      // ECM_PW_SW == 2 (OPR)
  bool    jmrSourceOn();      // CMDS_JMR_SOURCHE_SW == 1 (ON)
  bool    ecmXmitAft();       // ECM_XMIT_SW == 2 (AFT)
}
