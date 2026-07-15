#include "ui.h"
#include "settings_internal.h"

namespace SettingsInt {
lv_obj_t* host = nullptr;
lv_obj_t* makeSub() {
  lv_obj_t* s = lv_obj_create(host);
  lv_obj_set_size(s, lv_pct(100), lv_pct(100));
  UI::stripPanel(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_HIDDEN);
  return s;
}
void showSub(lv_obj_t* sub) {
  uint32_t n = lv_obj_get_child_count(host);
  for (uint32_t i = 0; i < n; i++)
    lv_obj_add_flag(lv_obj_get_child(host, i), LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(sub, LV_OBJ_FLAG_HIDDEN);
}
void showRoot() {}   // real implementation in Task 8
}

namespace PageSettings {
void build(lv_obj_t* parent) {
  SettingsInt::host = parent;
  UI::makeLabel(parent, "SETTINGS (stub)", &lv_font_montserrat_28, UI::colText());
}
void reset() { SettingsInt::showRoot(); }
void setDimPtr(int*) {}
void setSleepSecsPtr(int*) {}
}
