#include "ui.h"
#include "config.h"

namespace {
  lv_obj_t* s_grp[PageHome::STATE_COUNT];
  lv_obj_t* s_ssidLbl  = nullptr;
  lv_obj_t* s_fuelVal  = nullptr;
  void (*s_retryCb)()  = nullptr;
  void (*s_bleCb)()    = nullptr;

  lv_obj_t* makeGroup(lv_obj_t* parent) {
    lv_obj_t* g = lv_obj_create(parent);
    lv_obj_set_size(g, lv_pct(100), lv_pct(100));
    lv_obj_center(g);
    UI::stripPanel(g);
    lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);
    return g;
  }
}

namespace PageHome {

void build(lv_obj_t* parent) {
  lv_obj_t* g;

  g = makeGroup(parent); s_grp[NO_CREDS] = g;
  lv_obj_t* l = UI::makeLabel(g, "NO WIFI CREDENTIALS", &lv_font_montserrat_28, UI::colWarn());
  lv_obj_align(l, LV_ALIGN_CENTER, 0, -70);
  lv_obj_t* b = UI::makeButton(g, "Reboot into BLE Setup",
      [](lv_event_t*) { if (s_bleCb) s_bleCb(); }, nullptr);
  lv_obj_set_width(b, 320);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 20);

  g = makeGroup(parent); s_grp[WIFI_CONNECTING] = g;
  lv_obj_t* sp = lv_spinner_create(g);
  lv_obj_set_size(sp, 64, 64);
  lv_obj_align(sp, LV_ALIGN_CENTER, 0, -70);
  s_ssidLbl = UI::makeLabel(g, "Connecting...", &lv_font_montserrat_20, UI::colText());
  lv_obj_align(s_ssidLbl, LV_ALIGN_CENTER, 0, 10);

  g = makeGroup(parent); s_grp[WIFI_FAILED] = g;
  l = UI::makeLabel(g, "NO WIFI", &lv_font_montserrat_28, UI::colWarn());
  lv_obj_align(l, LV_ALIGN_CENTER, 0, -70);
  b = UI::makeButton(g, "Retry", [](lv_event_t*) { if (s_retryCb) s_retryCb(); }, nullptr);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 20);

  g = makeGroup(parent); s_grp[WAITING_DCS] = g;
  l = UI::makeLabel(g, "WAITING FOR DCS...", &lv_font_montserrat_28, UI::colText());
  lv_obj_center(l);

  g = makeGroup(parent); s_grp[AIRCRAFT] = g;
  l = UI::makeLabel(g, "FUEL", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(l, LV_ALIGN_CENTER, 0, -90);
  s_fuelVal = UI::makeLabel(g, "---", &lv_font_montserrat_48, UI::colText());
  lv_obj_align(s_fuelVal, LV_ALIGN_CENTER, 0, -10);

  setState(WIFI_CONNECTING);
}

void setState(State s) {
  for (int i = 0; i < STATE_COUNT; i++) {
    if (i == (int)s) lv_obj_remove_flag(s_grp[i], LV_OBJ_FLAG_HIDDEN);
    else             lv_obj_add_flag(s_grp[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void setSsid(const char* ssid) {
  lv_label_set_text_fmt(s_ssidLbl, "Connecting to %s...", ssid);
}

void setFuel(uint32_t lbs) {
  lv_label_set_text_fmt(s_fuelVal, "%u lbs", (unsigned)lbs);
}

void setRetryCb(void (*cb)())    { s_retryCb = cb; }
void setBleSetupCb(void (*cb)()) { s_bleCb = cb; }

}  // namespace PageHome
