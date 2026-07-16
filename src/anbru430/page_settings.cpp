#include "ui.h"
#include "settings_internal.h"
#include "display.h"
#include <Arduino.h>
#include <Preferences.h>

namespace {
  lv_obj_t* s_root      = nullptr;
  lv_obj_t* s_subBright = nullptr;
  lv_obj_t* s_slider    = nullptr;
  lv_obj_t* s_pctLbl    = nullptr;
  lv_obj_t* s_subSleep  = nullptr;
  lv_obj_t* s_sleepLbl  = nullptr;
  int* s_dimPtr   = nullptr;
  int* s_sleepPtr = nullptr;
  int  s_prevDim   = 0;
  int  s_editSleep = 45;

  lv_obj_t*   s_rebootModal = nullptr;
  lv_obj_t*   s_rebootLbl   = nullptr;
  lv_timer_t* s_rebootTimer = nullptr;
  int         s_rebootSecs  = 0;

  void sleepShow() {
    if (s_editSleep == 0) lv_label_set_text(s_sleepLbl, "Off");
    else lv_label_set_text_fmt(s_sleepLbl, "%d s", s_editSleep);
  }

  // Sleep adjust — same value walk as the OLED unit (10..120 in 5 s steps, then Off)
  void sleepStep(int dir) {
    if (dir > 0) {
      if (s_editSleep > 0 && s_editSleep < 120) s_editSleep += 5;
      else if (s_editSleep == 120) s_editSleep = 0;
    } else {
      if (s_editSleep == 0) s_editSleep = 120;
      else if (s_editSleep > 10) s_editSleep -= 5;
    }
    sleepShow();
  }

  void rebootCancel(lv_event_t*) {
    if (s_rebootTimer) { lv_timer_delete(s_rebootTimer); s_rebootTimer = nullptr; }
    if (s_rebootModal) { lv_obj_delete(s_rebootModal); s_rebootModal = nullptr; }
  }

  void rebootTick(lv_timer_t*) {
    if (--s_rebootSecs <= 0) {
      lv_timer_delete(s_rebootTimer);
      s_rebootTimer = nullptr;
      ESP.restart();
      return;
    }
    lv_label_set_text_fmt(s_rebootLbl, "Rebooting in %d...", s_rebootSecs);
  }

  void doReboot(lv_event_t*) {
    s_rebootSecs = 5;
    s_rebootModal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_rebootModal, Display::WIDTH, Display::HEIGHT);
    lv_obj_set_pos(s_rebootModal, 0, 0);
    UI::stripPanel(s_rebootModal);
    s_rebootLbl = UI::makeLabel(s_rebootModal, "Rebooting in 5...", &lv_font_montserrat_28, UI::colWarn());
    lv_obj_align(s_rebootLbl, LV_ALIGN_CENTER, 0, -40);
    lv_obj_t* c = UI::makeButton(s_rebootModal, "Cancel", rebootCancel, nullptr);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 60);
    s_rebootTimer = lv_timer_create(rebootTick, 1000, nullptr);
  }
}

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

void showRoot() { showSub(s_root); }

}  // namespace SettingsInt

namespace PageSettings {

void build(lv_obj_t* parent) {
  SettingsInt::host = parent;

  // Root list
  s_root = SettingsInt::makeSub();
  lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(s_root, 14, 0);
  UI::makeButton(s_root, "WiFi",       [](lv_event_t*) { SettingsWifi::open(); },   nullptr);
  UI::makeButton(s_root, "Update",     [](lv_event_t*) { SettingsUpdate::open(); }, nullptr);
  UI::makeButton(s_root, "Mouse Tune", [](lv_event_t*) { SettingsMouse::open(); },  nullptr);
  UI::makeButton(s_root, "Brightness", [](lv_event_t*) {
    s_prevDim = s_dimPtr ? *s_dimPtr : 0;
    UI::setDimLevel(s_prevDim);
    lv_slider_set_value(s_slider, 80 - s_prevDim, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_pctLbl, "%d%%", 100 - s_prevDim);
    SettingsInt::showSub(s_subBright);
  }, nullptr);
  UI::makeButton(s_root, "Sleep", [](lv_event_t*) {
    s_editSleep = s_sleepPtr ? *s_sleepPtr : 45;
    sleepShow();
    SettingsInt::showSub(s_subSleep);
  }, nullptr);
  UI::makeButton(s_root, "Reboot", doReboot, nullptr);

  // Brightness sub — slider drives the dim overlay live (higher = brighter)
  s_subBright = SettingsInt::makeSub();
  lv_obj_t* t = UI::makeLabel(s_subBright, "BRIGHTNESS", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 24);
  s_slider = lv_slider_create(s_subBright);
  lv_obj_set_size(s_slider, 560, 24);
  lv_obj_align(s_slider, LV_ALIGN_CENTER, 0, -30);
  lv_slider_set_range(s_slider, 0, 80);     // slider value = 80 - dim (right = bright)
  lv_obj_add_event_cb(s_slider, [](lv_event_t*) {
    int dim = 80 - (int)lv_slider_get_value(s_slider);
    UI::setDimLevel(dim);
    lv_label_set_text_fmt(s_pctLbl, "%d%%", 100 - dim);
  }, LV_EVENT_VALUE_CHANGED, nullptr);
  s_pctLbl = UI::makeLabel(s_subBright, "100%", &lv_font_montserrat_28, UI::colText());
  lv_obj_align(s_pctLbl, LV_ALIGN_CENTER, 0, 30);
  lv_obj_t* b = UI::makeButton(s_subBright, "Save", [](lv_event_t*) {
    int dim = 80 - (int)lv_slider_get_value(s_slider);
    if (s_dimPtr) *s_dimPtr = dim;
    Preferences p;
    p.begin("brew", false);
    p.putInt("dimlvl", dim);
    p.end();
    SettingsInt::showRoot();
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 60, -16);
  b = UI::makeButton(s_subBright, "Cancel", [](lv_event_t*) {
    UI::setDimLevel(s_prevDim);
    SettingsInt::showRoot();
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -60, -16);

  // Sleep sub
  s_subSleep = SettingsInt::makeSub();
  t = UI::makeLabel(s_subSleep, "LCD SLEEP", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 24);
  b = UI::makeButton(s_subSleep, "-", [](lv_event_t*) { sleepStep(-1); }, nullptr);
  lv_obj_set_size(b, 96, 96);
  lv_obj_align(b, LV_ALIGN_CENTER, -180, -10);
  s_sleepLbl = UI::makeLabel(s_subSleep, "45 s", &lv_font_montserrat_48, UI::colText());
  lv_obj_align(s_sleepLbl, LV_ALIGN_CENTER, 0, -10);
  b = UI::makeButton(s_subSleep, "+", [](lv_event_t*) { sleepStep(1); }, nullptr);
  lv_obj_set_size(b, 96, 96);
  lv_obj_align(b, LV_ALIGN_CENTER, 180, -10);
  b = UI::makeButton(s_subSleep, "Save", [](lv_event_t*) {
    if (s_sleepPtr) *s_sleepPtr = s_editSleep;
    Preferences p;
    p.begin("brew", false);
    p.putInt("sleep", s_editSleep);
    p.end();
    SettingsInt::showRoot();
  }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 60, -16);
  b = UI::makeButton(s_subSleep, "Cancel",
      [](lv_event_t*) { SettingsInt::showRoot(); }, nullptr);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -60, -16);

  SettingsWifi::build();
  SettingsUpdate::build();
  SettingsMouse::build();

  SettingsInt::showRoot();
}

void reset() {
  if (s_dimPtr) UI::setDimLevel(*s_dimPtr);
  SettingsInt::showRoot();
}
void setDimPtr(int* dim)        { s_dimPtr = dim; }
void setSleepSecsPtr(int* secs) { s_sleepPtr = secs; }

}  // namespace PageSettings
