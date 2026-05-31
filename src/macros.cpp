#include "macros.h"
#include "hid.h"
#include <Arduino.h>

// Brew370 defaults — different screen resolution from Sham_Master.
// Tune via Settings -> Mouse Tuning menu. Saved to NVS key "brew".
int mouseParams[6] = {460, 8, 555, 295, 10, 26};

static void openMapAndSelectPin() {
  HID::Keyboard.releaseAll();
  HID::Mouse.release(MOUSE_LEFT);
  HID::pressKey(KEY_F10);
  HID::homeMouse();
  HID::moveMouseTotal(mouseParams[0], mouseParams[1]);
  HID::moveMouseTotal(mouseParams[2], mouseParams[3]);
}

static void dropPinAndLabel(const char* label) {
  HID::mouseClick();
  delay(400);
  HID::moveMouseTotal(mouseParams[4], mouseParams[5]);
  HID::mouseClick();
  HID::Keyboard.releaseAll();
  HID::typeText(label);
  HID::Mouse.move(-60, 0);
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
  HID::Mouse.move(-60, 0);
  HID::mouseClick();
}

void executeMacro(int idx) {
  if (HID::isReady()) macros[idx].execute();
}
