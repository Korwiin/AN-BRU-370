#include "macros.h"
#include "hid.h"
#include <Arduino.h>

// Mouse calibration params — loaded from NVS in setup(), defaults here.
// [0]=aptX [1]=aptY : absolute position of Pin Tool button (0-32767)
// [2]=amcX [3]=amcY : absolute position of map drop target (0-32767)
// [4]=lbX  [5]=lbY  : absolute position of label input box (0-32767)
// [6]=cdX  [7]=cdY  : absolute position of CDRP confirm click target (0-32767)
int mouseParams[8] = {875, 50, 2048, 2048, 0, 0, 0, 0};

static void openMapAndSelectPin() {
  HID::Keyboard.releaseAll();
  HID::Keyboard.press(KEY_LEFT_CTRL);
  HID::Keyboard.press(KEY_F10);
  delay(50);
  HID::Keyboard.releaseAll();
  delay(30);
  HID::moveAbs((uint16_t)mouseParams[0], (uint16_t)mouseParams[1]);
  HID::mouseClick();
  HID::moveAbs((uint16_t)mouseParams[2], (uint16_t)mouseParams[3]);
}

static void dropPinAndLabel(const char* label) {
  HID::mouseClick();
  delay(400);
  HID::moveAbs((uint16_t)mouseParams[4], (uint16_t)mouseParams[5]);
  HID::mouseClick();
  HID::Keyboard.releaseAll();
  HID::typeText(label);
  HID::moveRel(-60, 0);
  HID::mouseClick();
  delay(250);
  HID::pressKey(KEY_F1);
}

static void executeAWACS()       { openMapAndSelectPin(); dropPinAndLabel("magic11"); }
static void executeFCAP()        { openMapAndSelectPin(); dropPinAndLabel("fcap"); }
static void executeREAPER()      { openMapAndSelectPin(); dropPinAndLabel("1688 reaper"); }
static void executeCREAPER()     { openMapAndSelectPin(); dropPinAndLabel("1688 CREAPER"); }

static void executeCDRP(int idx);
static void executeCDRPAlpha()   { executeCDRP(4); }
static void executeCDRPBravo()   { executeCDRP(5); }
static void executeCDRPCharlie() { executeCDRP(6); }
static void executeCDRPDelta()   { executeCDRP(7); }
static void executeCDRPEcho()    { executeCDRP(8); }
static void executeCDRPFoxtrot() { executeCDRP(9); }
static void executeCDRPGamma()   { executeCDRP(10); }

Macro macros[] = {
  {"AWACS",        "magic11",      executeAWACS},
  {"FCAP",         "fcap",         executeFCAP},
  {"REAPER",       "1688 reaper",  executeREAPER},
  {"CREAPER",      "1688 CREAPER", executeCREAPER},
  {"C130 A",   "CDRP-ALPHA",   executeCDRPAlpha},
  {"C130 B",   "CDRP-BETA",    executeCDRPBravo},
  {"C130 C",   "CDRP-CHARLIE", executeCDRPCharlie},
  {"C130 D",   "CDRP-DELTA",   executeCDRPDelta},
  {"C130 E",   "CDRP-ECHO",    executeCDRPEcho},
  {"C130 F",   "CDRP-FOXTROT", executeCDRPFoxtrot},
  {"C130 G",   "CDRP-GAMMA",   executeCDRPGamma},
};
const int numMacros = sizeof(macros) / sizeof(macros[0]);

static void executeCDRP(int idx) {
  HID::typeText(macros[idx].payload);
  HID::moveAbs((uint16_t)mouseParams[6], (uint16_t)mouseParams[7]);
  HID::mouseClick();
}

void executeMacro(int idx) {
  if (HID::isReady()) macros[idx].execute();
}
