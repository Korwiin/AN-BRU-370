#include "ui.h"

namespace {
  constexpr int NAV_H = 72;
  lv_obj_t* s_pages[UI::PAGE_COUNT];
  lv_obj_t* s_navBtns[UI::PAGE_COUNT];
  lv_obj_t* s_navLbls[UI::PAGE_COUNT];
  lv_obj_t* s_dim = nullptr;
  UI::Page  s_current = UI::PAGE_HOME;

  void navCb(lv_event_t* e) {
    UI::showPage((UI::Page)(uintptr_t)lv_event_get_user_data(e));
  }
}

namespace UI {

void stripPanel(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, colBg(), 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb, void* user) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_size(b, 220, 64);
  lv_obj_set_style_bg_color(b, colPanel(), 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
  lv_obj_center(l);
  return b;
}

void begin() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, colBg(), 0);
  lv_obj_set_style_text_color(scr, colText(), 0);
  lv_obj_set_style_text_font(scr, &lv_font_montserrat_20, 0);

  // Page containers — everything above the nav bar
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_t* pg = lv_obj_create(scr);
    lv_obj_set_size(pg, 800, 480 - NAV_H);
    lv_obj_set_pos(pg, 0, 0);
    lv_obj_set_style_pad_all(pg, 8, 0);
    stripPanel(pg);
    lv_obj_add_flag(pg, LV_OBJ_FLAG_HIDDEN);
    s_pages[i] = pg;
  }

  // Bottom nav bar
  lv_obj_t* nav = lv_obj_create(scr);
  lv_obj_set_size(nav, 800, NAV_H);
  lv_obj_set_pos(nav, 0, 480 - NAV_H);
  lv_obj_set_style_pad_all(nav, 4, 0);
  lv_obj_set_style_pad_gap(nav, 4, 0);
  stripPanel(nav);
  lv_obj_set_style_bg_color(nav, lv_color_hex(0x080A0C), 0);
  lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
  static const char* kNames[PAGE_COUNT] = { "HOME", "MACROS", "SETTINGS", "STATUS" };
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_t* b = lv_button_create(nav);
    lv_obj_set_height(b, NAV_H - 8);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_add_event_cb(b, navCb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, kNames[i]);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_center(l);
    s_navBtns[i] = b;
    s_navLbls[i] = l;
  }

  PageHome::build(s_pages[PAGE_HOME]);
  PageMacros::build(s_pages[PAGE_MACROS]);
  PageSettings::build(s_pages[PAGE_SETTINGS]);
  PageStatus::build(s_pages[PAGE_STATUS]);

  // Dim overlay — topmost layer, ignores touches (they pass through)
  s_dim = lv_obj_create(lv_layer_sys());
  lv_obj_set_size(s_dim, 800, 480);
  lv_obj_set_pos(s_dim, 0, 0);
  lv_obj_set_style_bg_color(s_dim, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_dim, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_dim, 0, 0);
  lv_obj_set_style_radius(s_dim, 0, 0);
  lv_obj_remove_flag(s_dim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_dim, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_remove_flag(s_dim, LV_OBJ_FLAG_SCROLLABLE);

  showPage(PAGE_HOME);
}

void showPage(Page p) {
  if (s_current == PAGE_SETTINGS && p != PAGE_SETTINGS) PageSettings::reset();
  for (int i = 0; i < PAGE_COUNT; i++) {
    if (i == (int)p) lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    else             lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(s_navBtns[i], i == (int)p ? colAccent() : colPanel(), 0);
    lv_obj_set_style_text_color(s_navLbls[i], i == (int)p ? lv_color_black() : colText(), 0);
  }
  s_current = p;
}

Page currentPage() { return s_current; }

void setDimLevel(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 80) pct = 80;
  lv_obj_set_style_bg_opa(s_dim, (lv_opa_t)((pct * 255) / 100), 0);
}

}  // namespace UI
