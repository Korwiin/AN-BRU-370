#include <Arduino.h>
#include <USB.h>
#include "config.h"
#include "pins.h"
#include "display.h"
#include "esp_heap_caps.h"

static uint16_t* s_line = nullptr;  // one solid line, drawn HEIGHT times

static void fillScreen(uint16_t rgb565) {
  for (int x = 0; x < Display::WIDTH; x++) s_line[x] = rgb565;
  for (int y = 0; y < Display::HEIGHT; y++) {
    esp_lcd_panel_draw_bitmap(Display::panel(), 0, y, Display::WIDTH, y + 1, s_line);
  }
}

void setup() {
  USB.manufacturerName("E4 Mafia");
  USB.productName(USB_PRODUCT_NAME);
  USB.PID(USB_PID);
  USB.firmwareVersion(FIRMWARE_VERSION_INT);
  USB.begin();

  Serial.begin(115200);
  while (!Serial && millis() < 2000) { }
  Serial.println("=== ANBRU-430 boot ===");

  s_line = (uint16_t*)heap_caps_malloc(Display::WIDTH * 2, MALLOC_CAP_DMA);
  if (!Display::begin() || !s_line) {
    Serial.println("DISPLAY INIT FAILED");
    return;
  }
  Serial.println("display up");
}

void loop() {
  static const uint16_t colors[] = { 0xF800 /*red*/, 0x07E0 /*green*/, 0x001F /*blue*/ };
  static int idx = 0;
  static unsigned long last = 0;
  if (Display::panel() && millis() - last >= 1000) {
    last += 1000;
    fillScreen(colors[idx]);
    idx = (idx + 1) % 3;
  }
}
