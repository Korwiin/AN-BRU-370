#include "macros.h"
#include "hid.h"
#include <Arduino.h>

// Mouse calibration params — loaded from NVS in setup(), defaults here.
// [0]=aptX [1]=aptY : absolute position of Pin Tool button (0-32767)
// [2]=amcX [3]=amcY : absolute position of map drop target (0-32767)
// [4]=lbX  [5]=lbY  : relative delta from pin drop to label input
int mouseParams[6] = {16384, 1000, 16384, 16384, 10, 26};

static void openMapAndSelectPin() {
  HID::Keyboard.releaseAll();
  HID::pressKey(KEY_F10);
  HID::moveAbs((uint16_t)mouseParams[0], (uint16_t)mouseParams[1]);
  HID::moveAbs((uint16_t)mouseParams[2], (uint16_t)mouseParams[3]);
}

static void dropPinAndLabel(const char* label) {
  HID::mouseClick();
  delay(400);
  HID::moveRel((int16_t)mouseParams[4], (int16_t)mouseParams[5]);
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

static void executeCDRP(int idx);
static void executeCDRPAlpha()   { executeCDRP(3); }
static void executeCDRPBravo()   { executeCDRP(4); }
static void executeCDRPCharlie() { executeCDRP(5); }
static void executeCDRPDelta()   { executeCDRP(6); }
static void executeCDRPEcho()    { executeCDRP(7); }
static void executeCDRPFoxtrot() { executeCDRP(8); }
static void executeCDRPGamma()   { executeCDRP(9); }

Macro macros[] = {
  {"AWACS",        "magic11",      executeAWACS},
  {"FCAP",         "fcap",         executeFCAP},
  {"REAPER",       "1688 reaper",  executeREAPER},
  {"CDRP ALPHA",   "CDRP-ALPHA",   executeCDRPAlpha},
  {"CDRP BRAVO",   "CDRP-BETA",    executeCDRPBravo},
  {"CDRP CHARLIE", "CDRP-CHARLIE", executeCDRPCharlie},
  {"CDRP DELTA",   "CDRP-DELTA",   executeCDRPDelta},
  {"CDRP ECHO",    "CDRP-ECHO",    executeCDRPEcho},
  {"CDRP FOXTROT", "CDRP-FOXTROT", executeCDRPFoxtrot},
  {"CDRP GAMMA",   "CDRP-GAMMA",   executeCDRPGamma},
};
const int numMacros = sizeof(macros) / sizeof(macros[0]);

static void executeCDRP(int idx) {
  HID::typeText(macros[idx].payload);
  HID::moveAbs(100, 100);
  HID::mouseClick();
}

void executeMacro(int idx) {
  if (HID::isReady()) macros[idx].execute();
}
