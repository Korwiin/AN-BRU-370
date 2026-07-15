#include "ui.h"
#include "settings_internal.h"
#include "macros.h"
#include "hid.h"
#include <Arduino.h>
#include <Preferences.h>
#include <stdlib.h>

namespace {
  lv_obj_t* s_subRoot = nullptr;
  lv_obj_t* s_subSize = nullptr;
  lv_obj_t* s_taW     = nullptr;
  lv_obj_t* s_taH     = nullptr;
  lv_obj_t* s_kb      = nullptr;
  lv_obj_t* s_sizeErr = nullptr;
  lv_obj_t* s_subCal   = nullptr;
  lv_obj_t* s_calTitle = nullptr;
  lv_obj_t* s_calXLbl  = nullptr;
  lv_obj_t* s_calYLbl  = nullptr;
  int      s_calibIdx = 0;
  uint16_t s_calX = 0, s_calY = 0;
  uint16_t s_repeats = 0;
  int* s_scrW = nullptr;
  int* s_scrH = nullptr;

  const char* kTargets[4] = { "Pin Tool", "Map Center", "Text Field", "Click Out" };

  void calShow() {
    lv_label_set_text_fmt(s_calXLbl, "X: %u", s_calX);
    lv_label_set_text_fmt(s_calYLbl, "Y: %u", s_calY);
  }

  void openCal(int ci) {
    if (ci == 2) {  // Text Field needs the map + pin-tool pre-sequence (OLED parity)
      HID::Keyboard.releaseAll();
      HID::pressKey(KEY_F10);
      delay(30);
      HID::moveAbs((uint16_t)mouseParams[0], (uint16_t)mouseParams[1]);
      HID::mouseClick();
      HID::moveAbs((uint16_t)mouseParams[2], (uint16_t)mouseParams[3]);
      HID::mouseClick();
      delay(400);
    }
    s_calibIdx = ci;
    s_calX = (uint16_t)mouseParams[ci * 2];
    s_calY = (uint16_t)mouseParams[ci * 2 + 1];
    s_repeats = 0;
    HID::moveAbs(s_calX, s_calY);
    lv_label_set_text_fmt(s_calTitle, "MOVE TO:  %s", kTargets[ci]);
    calShow();
    SettingsInt::showSub(s_subCal);
  }

  void stepCal(int dx, int dy) {
    // Hold-to-repeat acceleration: LVGL fires LONG_PRESSED_REPEAT ~every 100 ms
    int step = (s_repeats < 8) ? 1 : (s_repeats < 25) ? 5 : (s_repeats < 60) ? 20 : 60;
    int w = s_scrW ? *s_scrW : 1920;
    int h = s_scrH ? *s_scrH : 1080;
    s_calX = (uint16_t)constrain((int)s_calX + dx * step, 0, w - 1);
    s_calY = (uint16_t)constrain((int)s_calY + dy * step, 0, h - 1);
    HID::moveAbs(s_calX, s_calY);
    calShow();
  }

  void dpadCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    int dir = (int)(uintptr_t)lv_event_get_user_data(e);  // 0=L 1=R 2=U 3=D
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
      s_repeats = 0;
      return;
    }
    if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_LONG_PRESSED_REPEAT) return;
    if (code == LV_EVENT_LONG_PRESSED_REPEAT) s_repeats++;
    switch (dir) {
      case 0: stepCal(-1, 0); break;
      case 1: stepCal( 1, 0); break;
      case 2: stepCal(0, -1); break;
      case 3: stepCal(0,  1); break;
    }
  }

  lv_obj_t* makeDpadBtn(lv_obj_t* parent, const char* sym, int dir,
                        lv_align_t align, int xo, int yo) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, 96, 96);
    lv_obj_set_style_bg_color(b, UI::colPanel(), 0);
    lv_obj_align(b, align, xo, yo);
    lv_obj_add_event_cb(b, dpadCb, LV_EVENT_ALL, (void*)(uintptr_t)dir);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
    lv_obj_center(l);
    return b;
  }

  void saveCal(lv_event_t*) {
    mouseParams[s_calibIdx * 2]     = (int)s_calX;
    mouseParams[s_calibIdx * 2 + 1] = (int)s_calY;
    Preferences p;
    p.begin("brew", false);
    switch (s_calibIdx) {
      case 0: p.putInt("apxX",  mouseParams[0]); p.putInt("apxY",  mouseParams[1]); break;
      case 1: p.putInt("amcX2", mouseParams[2]); p.putInt("amcY2", mouseParams[3]); break;
      case 2: p.putInt("lbX2",  mouseParams[4]); p.putInt("lbY2",  mouseParams[5]); break;
      case 3: p.putInt("cdrpX", mouseParams[6]); p.putInt("cdrpY", mouseParams[7]); break;
    }
    p.end();
    SettingsInt::showSub(s_subRoot);
  }

  void rootCalCb(lv_event_t* e) {
    openCal((int)(uintptr_t)lv_event_get_user_data(e));
  }

  void saveSize(lv_event_t*) {
    int w = atoi(lv_textarea_get_text(s_taW));
    int h = atoi(lv_textarea_get_text(s_taH));
    if (w < 320 || w > 7680 || h < 200 || h > 4320) {
      lv_label_set_text(s_sizeErr, "Invalid size");
      return;
    }
    Preferences p;
    p.begin("brew", false);
    p.putInt("scrW", w);
    p.putInt("scrH", h);
    p.end();
    // HID digitizer logical range is set at USB init — reboot to apply (OLED parity)
    lv_label_set_text(s_sizeErr, "Saved. Rebooting...");
    lv_refr_now(nullptr);
    delay(800);
    ESP.restart();
  }

  void kbFocusCb(lv_event_t* e) {
    lv_keyboard_set_textarea(s_kb, lv_event_get_target_obj(e));
  }
}

namespace SettingsMouse {

void build() {
  // Root: screen size + four targets
  s_subRoot = SettingsInt::makeSub();
  lv_obj_set_flex_flow(s_subRoot, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(s_subRoot, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(s_subRoot, 14, 0);
  UI::makeButton(s_subRoot, "Screen Size", [](lv_event_t*) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", s_scrW ? *s_scrW : 1920);
    lv_textarea_set_text(s_taW, buf);
    snprintf(buf, sizeof(buf), "%d", s_scrH ? *s_scrH : 1080);
    lv_textarea_set_text(s_taH, buf);
    lv_label_set_text(s_sizeErr, "");
    SettingsInt::showSub(s_subSize);
  }, nullptr);
  for (int i = 0; i < 4; i++)
    UI::makeButton(s_subRoot, kTargets[i], rootCalCb, (void*)(uintptr_t)i);
  UI::makeButton(s_subRoot, "Back",
      [](lv_event_t*) { SettingsInt::showRoot(); }, nullptr);

  // Screen size editor
  s_subSize = SettingsInt::makeSub();
  lv_obj_t* t = UI::makeLabel(s_subSize, "PC SCREEN SIZE", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 16, 12);
  lv_obj_t* l = UI::makeLabel(s_subSize, "W", &lv_font_montserrat_20, UI::colText());
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 30, 70);
  s_taW = lv_textarea_create(s_subSize);
  lv_textarea_set_one_line(s_taW, true);
  lv_textarea_set_accepted_chars(s_taW, "0123456789");
  lv_textarea_set_max_length(s_taW, 4);
  lv_obj_set_width(s_taW, 130);
  lv_obj_align(s_taW, LV_ALIGN_TOP_LEFT, 60, 60);
  lv_obj_add_event_cb(s_taW, kbFocusCb, LV_EVENT_FOCUSED, nullptr);
  l = UI::makeLabel(s_subSize, "H", &lv_font_montserrat_20, UI::colText());
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 220, 70);
  s_taH = lv_textarea_create(s_subSize);
  lv_textarea_set_one_line(s_taH, true);
  lv_textarea_set_accepted_chars(s_taH, "0123456789");
  lv_textarea_set_max_length(s_taH, 4);
  lv_obj_set_width(s_taH, 130);
  lv_obj_align(s_taH, LV_ALIGN_TOP_LEFT, 250, 60);
  lv_obj_add_event_cb(s_taH, kbFocusCb, LV_EVENT_FOCUSED, nullptr);
  s_sizeErr = UI::makeLabel(s_subSize, "", &lv_font_montserrat_20, UI::colWarn());
  lv_obj_align(s_sizeErr, LV_ALIGN_TOP_LEFT, 420, 70);
  lv_obj_t* b = UI::makeButton(s_subSize, "Save", saveSize, nullptr);
  lv_obj_set_size(b, 150, 52);
  lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -180, 60);
  b = UI::makeButton(s_subSize, "Cancel",
      [](lv_event_t*) { SettingsInt::showSub(s_subRoot); }, nullptr);
  lv_obj_set_size(b, 150, 52);
  lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -16, 60);
  s_kb = lv_keyboard_create(s_subSize);
  lv_keyboard_set_mode(s_kb, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_set_size(s_kb, lv_pct(100), 220);
  lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(s_kb, s_taW);

  // Calibration screen — D-pad with hold-to-accelerate, live HID moveAbs
  s_subCal = SettingsInt::makeSub();
  s_calTitle = UI::makeLabel(s_subCal, "MOVE TO:", &lv_font_montserrat_28, UI::colAccent());
  lv_obj_align(s_calTitle, LV_ALIGN_TOP_MID, 0, 12);
  s_calXLbl = UI::makeLabel(s_subCal, "X: 0", &lv_font_montserrat_28, UI::colText());
  lv_obj_align(s_calXLbl, LV_ALIGN_LEFT_MID, 40, -30);
  s_calYLbl = UI::makeLabel(s_subCal, "Y: 0", &lv_font_montserrat_28, UI::colText());
  lv_obj_align(s_calYLbl, LV_ALIGN_LEFT_MID, 40, 30);
  makeDpadBtn(s_subCal, LV_SYMBOL_LEFT,  0, LV_ALIGN_CENTER,  60,   20);
  makeDpadBtn(s_subCal, LV_SYMBOL_RIGHT, 1, LV_ALIGN_CENTER, 280,   20);
  makeDpadBtn(s_subCal, LV_SYMBOL_UP,    2, LV_ALIGN_CENTER, 170,  -80);
  makeDpadBtn(s_subCal, LV_SYMBOL_DOWN,  3, LV_ALIGN_CENTER, 170,  120);
  b = UI::makeButton(s_subCal, "Save", saveCal, nullptr);
  lv_obj_set_size(b, 150, 52);
  lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 16, -12);
  b = UI::makeButton(s_subCal, "Cancel",
      [](lv_event_t*) { SettingsInt::showSub(s_subRoot); }, nullptr);
  lv_obj_set_size(b, 150, 52);
  lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
}

void open() { SettingsInt::showSub(s_subRoot); }

void setScreenPtrs(int* w, int* h) {
  s_scrW = w;
  s_scrH = h;
}

}  // namespace SettingsMouse
