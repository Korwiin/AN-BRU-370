#include "ui.h"

namespace PageStatus {
void build(lv_obj_t* parent) {
  UI::makeLabel(parent, "STATUS (stub)", &lv_font_montserrat_28, UI::colText());
}
void refresh() {}
}
