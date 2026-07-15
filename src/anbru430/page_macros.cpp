#include "ui.h"
#include "macros.h"

namespace {
  void macroCb(lv_event_t* e) {
    executeMacro((int)(uintptr_t)lv_event_get_user_data(e));
  }
}

namespace PageMacros {

void build(lv_obj_t* parent) {
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_gap(parent, 10, 0);
  for (int i = 0; i < numMacros; i++) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, 180, 88);
    lv_obj_set_style_bg_color(b, UI::colPanel(), 0);
    lv_obj_add_event_cb(b, macroCb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, macros[i].name);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_center(l);
  }
}

}  // namespace PageMacros
