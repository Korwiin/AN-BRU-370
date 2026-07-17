#include "ui.h"
#include <string.h>

namespace {
  lv_obj_t* s_modal  = nullptr;
  lv_obj_t* s_title  = nullptr;
  lv_obj_t* s_sub    = nullptr;
  lv_obj_t* s_btnRow = nullptr;
  Alerts::Kind s_kind = Alerts::NONE;
  bool s_flash = false;
  void (*s_tapCb)()      = nullptr;
  void (*s_runSetupCb)() = nullptr;
  void (*s_mwsCb)()      = nullptr;
  char s_chaff[8] = "    ";
  char s_lastChaff[8] = "";  // chaff text last actually applied (CHAFF redraw gate)

  // setSetupProgress() idempotency gate — last-applied (step, maxStep, blinkOn).
  uint8_t s_setupStep    = 0xFF;
  uint8_t s_setupMaxStep = 0xFF;
  bool    s_setupBlinkOn = false;

  void ensureBuilt() {
    if (s_modal) return;
    s_modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_modal, 800, 480);
    lv_obj_set_pos(s_modal, 0, 0);
    UI::stripPanel(s_modal);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_modal, [](lv_event_t*) { if (s_tapCb) s_tapCb(); },
                        LV_EVENT_CLICKED, nullptr);

    s_title = UI::makeLabel(s_modal, "", &lv_font_montserrat_48, UI::colText());
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, -60);
    s_sub = UI::makeLabel(s_modal, "", &lv_font_montserrat_20, UI::colText());
    lv_obj_align(s_sub, LV_ALIGN_CENTER, 0, 20);

    s_btnRow = lv_obj_create(s_modal);
    lv_obj_set_size(s_btnRow, 560, 80);
    lv_obj_align(s_btnRow, LV_ALIGN_BOTTOM_MID, 0, -24);
    UI::stripPanel(s_btnRow);
    lv_obj_set_style_bg_opa(s_btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(s_btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_btnRow, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    UI::makeButton(s_btnRow, "RUN SETUP",
        [](lv_event_t*) { if (s_runSetupCb) s_runSetupCb(); }, nullptr);
    UI::makeButton(s_btnRow, "SET MWS ON",
        [](lv_event_t*) { if (s_mwsCb) s_mwsCb(); }, nullptr);
    lv_obj_add_flag(s_btnRow, LV_OBJ_FLAG_HIDDEN);
  }

  // flashOn: colored background + black text. Off: black background + colored text.
  void apply(const char* title, const char* sub, lv_color_t col, bool flashOn, bool buttons) {
    lv_obj_set_style_bg_color(s_modal, flashOn ? col : lv_color_black(), 0);
    lv_color_t tc = flashOn ? lv_color_black() : col;
    lv_obj_set_style_text_color(s_title, tc, 0);
    lv_obj_set_style_text_color(s_sub, tc, 0);
    lv_label_set_text(s_title, title);
    lv_label_set_text(s_sub, sub);
    if (buttons) lv_obj_remove_flag(s_btnRow, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_btnRow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_modal);  // outrank later lv_layer_top() siblings (confirm dialogs, countdowns)
  }
}

namespace Alerts {

void show(Kind k, bool flashOn) {
  ensureBuilt();
  bool same = (k == s_kind && flashOn == s_flash);
  if (same && k != CHAFF) return;  // no visual change
  if (same && k == CHAFF && strcmp(s_chaff, s_lastChaff) == 0) return;  // chaff text unchanged
  s_kind = k;
  s_flash = flashOn;
  switch (k) {
    case MASTER_CAUTION:
      apply("MASTER CAUTION", "Tap to reset", UI::colWarn(), flashOn, false);
      break;
    case STORES_CONFIG:
      apply("STORES CONFIG", "Tap to toggle CAT switch", UI::colWarn(), flashOn, false);
      break;
    case MISSILE:
      apply("MISSILE LAUNCH", "Tap to dispense", UI::colAlert(), flashOn, false);
      break;
    case CHAFF: {
      char buf[16];
      snprintf(buf, sizeof(buf), "CHAFF %s", s_chaff);
      apply(buf, "Tap to dispense", UI::colAlert(), flashOn, false);
      strlcpy(s_lastChaff, s_chaff, sizeof(s_lastChaff));
      break;
    }
    case NOT_READY_ALERT:
      apply("NOT READY", "Aircraft is not configured", UI::colWarn(), flashOn, true);
      break;
    default:
      break;
  }
}

void setChaff(const char* c) { strlcpy(s_chaff, c, sizeof(s_chaff)); }

void setSetupProgress(uint8_t step, uint8_t maxStep, bool blinkOn) {
  ensureBuilt();
  if (s_kind == SETUP_PROGRESS && step == s_setupStep &&
      maxStep == s_setupMaxStep && blinkOn == s_setupBlinkOn) return;  // no visual change
  s_kind = SETUP_PROGRESS;
  s_setupStep = step;
  s_setupMaxStep = maxStep;
  s_setupBlinkOn = blinkOn;
  lv_obj_set_style_bg_color(s_modal, lv_color_black(), 0);
  lv_obj_set_style_text_color(s_title, blinkOn ? UI::colAccent() : UI::colPanel(), 0);
  lv_obj_set_style_text_color(s_sub, UI::colText(), 0);
  lv_label_set_text_fmt(s_title, "SETUP  %u / %u", step, maxStep);
  lv_label_set_text(s_sub, "Configuring aircraft...");
  lv_obj_add_flag(s_btnRow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_modal);  // outrank later lv_layer_top() siblings (confirm dialogs, countdowns)
}

void hide() {
  if (s_modal) lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
  s_kind = NONE;
}

Kind current() { return s_kind; }

void setTapCb(void (*cb)()) { s_tapCb = cb; }

void setNotReadyCbs(void (*runSetup)(), void (*mwsOn)()) {
  s_runSetupCb = runSetup;
  s_mwsCb = mwsOn;
}

}  // namespace Alerts
