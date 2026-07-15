#include "ui.h"

namespace PageHome {
void build(lv_obj_t* parent) {
  UI::makeLabel(parent, "HOME (stub)", &lv_font_montserrat_28, UI::colText());
}
void setState(State) {}
void setSsid(const char*) {}
void setFuel(uint32_t) {}
void setRetryCb(void (*)()) {}
void setBleSetupCb(void (*)()) {}
}
