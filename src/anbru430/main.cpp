#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "lvgl_port.h"
#include "touch.h"
#include "ui.h"
#include "wifi_mgr.h"
#include "shell.h"

void setup() {
  Serial.begin(115200);
#ifdef DEV_BUILD
  while (!Serial && millis() < 2000) {}
  Serial.printf("=== ANBRU-430 v%s boot (dev, ui-skeleton) ===\n", FIRMWARE_VERSION);
#endif

  if (!Display::begin() || !LvglPort::begin()) return;
  UI::begin();
  Touch::begin();

#ifdef DEV_BUILD
  {
    static auto modeIdFn   = []() -> uint8_t { return (uint8_t)UI::currentPage(); };
    static auto modeNameFn = []() -> const char* { return "SKELETON"; };
    static auto menuSelFn  = []() -> int { return (int)UI::currentPage(); };
    static auto wifiBootFn = []() { WifiMgr::beginConnect(true); };
    static auto injectFn   = [](const char* verb, const char* rest) -> bool {
      if (strcmp(verb, "touch") != 0) return false;
      int x = -1, y = -1;
      if (sscanf(rest, "%d %d", &x, &y) != 2) return false;
      if (x < 0 || x >= 800 || y < 0 || y >= 480) return false;
      Touch::inject((uint16_t)x, (uint16_t)y);
      return true;
    };
    Shell::begin(Shell::Hooks{ modeIdFn, modeNameFn, menuSelFn, wifiBootFn,
                               injectFn, nullptr });
  }
#endif
}

void loop() {
  Shell::poll();
  LvglPort::loop();
}
