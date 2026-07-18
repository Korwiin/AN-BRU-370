#pragma once

// ANBRU-430 — Waveshare ESP32-S3-Touch-LCD-4.3B-BOX

#define FIRMWARE_VERSION     "0.05"
#define FIRMWARE_VERSION_INT  5  // plain decimal — keep in sync with FIRMWARE_VERSION

#define DEVICE_HOSTNAME  "ANBRU-430"
#define USB_PRODUCT_NAME "ANBRU-430"
#undef  USB_PID  // pins_arduino.h (esp32-s3-devkitc-1 variant) defines 0x1001 first
#define USB_PID          0x430A
#define BLE_DEVICE_NAME  "ANBRU-430"   // BLE setup advertising name

// DCS-BIOS network (same multicast group as Brew370)
#define DCSBIOS_MCAST_ADDR  "239.255.50.10"
#define DCSBIOS_MCAST_PORT  5010
#define DCSBIOS_CMD_PORT    7778

// OTA updates — ANBRU-430's own channel; Brew370's manifest.json is separate
#define OTA_MANIFEST_URL    "https://raw.githubusercontent.com/Korwiin/AN-BRU-370/main/ota/manifest-anbru430.json"
