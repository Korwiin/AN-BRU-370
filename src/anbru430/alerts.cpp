#include "ui.h"

namespace Alerts {
void show(Kind, bool) {}
void setChaff(const char*) {}
void setSetupProgress(uint8_t, uint8_t, bool) {}
void hide() {}
Kind current() { return NONE; }
void setTapCb(void (*)()) {}
void setNotReadyCbs(void (*)(), void (*)()) {}
}
