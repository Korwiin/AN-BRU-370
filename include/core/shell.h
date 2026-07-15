#pragma once
#include <Arduino.h>

// DEV_BUILD-only serial test shell (HWCDC). Line-based commands; every
// response line starts with '#'; each command ends with '#ok' or '#err <reason>'.
// Production builds compile poll() to a no-op (pattern follows hid.h).

#ifdef DEV_BUILD
namespace Shell {
  struct Hooks {
    uint8_t     (*modeId)();    // current MenuState as integer
    const char* (*modeName)();  // current MenuState as string
    int         (*menuSel)();   // settings menu selection index
    void        (*wifiBoot)();  // run the production boot-connect sequence
    // Device-specific test hooks — may be null:
    bool        (*injectInput)(const char* verb, const char* rest);  // "enc"/"touch" payloads
    const uint8_t* (*frameBuffer)(uint16_t* len);   // "fb?" dump source
  };
  void begin(const Hooks& hooks);   // call once in setup() after Serial.begin
  void poll();  // call once per loop() pass AND inside every blocking input loop
}
#else
namespace Shell {
  inline void poll() {}
}
#endif
