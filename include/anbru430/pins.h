#pragma once
#include <Arduino.h>

// ANBRU-430 pin map — Waveshare ESP32-S3-Touch-LCD-4.3B (wiki pin table).
// The RGB panel + I2C + USB consume nearly every GPIO; there are no spares.
namespace Pins {
  // I2C bus (GT911 touch + CH422G expander + RTC)
  constexpr uint8_t I2C_SDA = 8;
  constexpr uint8_t I2C_SCL = 9;
  constexpr uint8_t TP_IRQ  = 4;   // GT911 interrupt (polling driver ignores it)

  // RGB panel sync/clock
  constexpr int8_t LCD_VSYNC = 3;
  constexpr int8_t LCD_HSYNC = 46;
  constexpr int8_t LCD_DE    = 5;
  constexpr int8_t LCD_PCLK  = 7;

  // RGB565 data lines, esp_lcd order data0..data15 = B3..B7, G2..G7, R3..R7
  constexpr int8_t LCD_DATA[16] = {
    14, 38, 18, 17, 10,        // B3 B4 B5 B6 B7
    39,  0, 45, 48, 47, 21,    // G2 G3 G4 G5 G6 G7
     1,  2, 42, 41, 40         // R3 R4 R5 R6 R7
  };

  // CH422G expander outputs (EXIO bit numbers, not GPIOs)
  constexpr uint8_t EXIO_TP_RST  = 1;
  constexpr uint8_t EXIO_LCD_BL  = 2;
  constexpr uint8_t EXIO_LCD_RST = 3;
  constexpr uint8_t EXIO_SD_CS   = 4;
  constexpr uint8_t EXIO_USB_SEL = 5;  // low = USB mode (4.3 non-B; presence on B unverified)
}
