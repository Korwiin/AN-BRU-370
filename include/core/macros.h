#pragma once

struct Macro {
  const char* name;
  const char* payload;
  void (*execute)();
};

extern Macro     macros[];
extern const int numMacros;

// Mouse calibration params — loaded from NVS in setup(), defaults here
// These are Brew370-specific — tune via Settings -> Mouse Tuning
extern int mouseParams[8];  // ptX, ptY, mcX, mcY, lbX, lbY, cdX, cdY

void executeMacro(int idx);
