#include "display.h"
#include "ch422g.h"
#include "pins.h"
#include <Wire.h>
#include "esp_lcd_panel_rgb.h"

namespace {
  esp_lcd_panel_handle_t s_panel = nullptr;
  uint8_t s_exioMask = 0;
}

namespace Display {

bool begin() {
  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL, 400000);
  if (!CH422G::begin()) return false;

  // TP_RST + LCD_RST low (reset), BL off, SD_CS high (deselect), USB_SEL low (USB mode)
  s_exioMask = (1u << Pins::EXIO_SD_CS);
  CH422G::setOutputs(s_exioMask);
  delay(20);
  // Release resets, keep BL off until first frame is drawn
  s_exioMask = (1u << Pins::EXIO_SD_CS) |
               (1u << Pins::EXIO_TP_RST) |
               (1u << Pins::EXIO_LCD_RST);
  CH422G::setOutputs(s_exioMask);
  delay(120);

  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = 14000000;  // E2: was 16MHz — more bounce-buffer refill slack
  cfg.timings.h_res = WIDTH;
  cfg.timings.v_res = HEIGHT;
  cfg.timings.hsync_pulse_width = 4;
  cfg.timings.hsync_back_porch  = 8;
  cfg.timings.hsync_front_porch = 8;
  cfg.timings.vsync_pulse_width = 4;
  cfg.timings.vsync_back_porch  = 8;
  cfg.timings.vsync_front_porch = 8;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 2;                          // double FB in PSRAM
  cfg.bounce_buffer_size_px = WIDTH * 20;   // more refill slack per chunk — mitigates left-edge flicker from PSRAM/cache contention
  cfg.psram_trans_align = 64;
  cfg.hsync_gpio_num = Pins::LCD_HSYNC;
  cfg.vsync_gpio_num = Pins::LCD_VSYNC;
  cfg.de_gpio_num    = Pins::LCD_DE;
  cfg.pclk_gpio_num  = Pins::LCD_PCLK;
  cfg.disp_gpio_num  = -1;
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = Pins::LCD_DATA[i];
  cfg.flags.fb_in_psram = 1;

  if (esp_lcd_new_rgb_panel(&cfg, &s_panel) != ESP_OK) return false;
  if (esp_lcd_panel_reset(s_panel) != ESP_OK) return false;
  if (esp_lcd_panel_init(s_panel) != ESP_OK) return false;

  // Backlight on
  s_exioMask = (1u << Pins::EXIO_SD_CS) |
               (1u << Pins::EXIO_TP_RST) |
               (1u << Pins::EXIO_LCD_RST) |
               (1u << Pins::EXIO_LCD_BL);
  CH422G::setOutputs(s_exioMask);
  return true;
}

esp_lcd_panel_handle_t panel() { return s_panel; }

void setBacklight(bool on) {
  if (on)  s_exioMask |=  (1u << Pins::EXIO_LCD_BL);
  else     s_exioMask &= ~(1u << Pins::EXIO_LCD_BL);
  CH422G::setOutputs(s_exioMask);
}

}  // namespace Display
