#pragma once
#include "ui.h"

// Shared plumbing for the Settings page family (page_settings.cpp,
// settings_wifi.cpp, settings_update.cpp, settings_mouse.cpp).
namespace SettingsInt {
  extern lv_obj_t* host;        // the PAGE_SETTINGS container
  lv_obj_t* makeSub();          // create a hidden full-size sub-screen inside host
  void showSub(lv_obj_t* sub);  // show one direct child of host, hide the rest
  void showRoot();              // back to the root list
}
