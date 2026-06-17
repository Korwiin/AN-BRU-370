#pragma once

// Copy this file to config.h and fill in your values.
// config.h is gitignored — never commit credentials.

// Wi-Fi credentials are loaded from NVS (Preferences) only — no compiled-in defaults.
// Use Settings → Wi-Fi → Secrets → BLE TERM to configure at runtime.

// DCS-BIOS network
#define DCSBIOS_MCAST_ADDR  "239.255.50.10"
#define DCSBIOS_MCAST_PORT  5010
#define DCSBIOS_CMD_PORT    7778

// Device identity
#define FIRMWARE_VERSION     "0.56"
#define FIRMWARE_VERSION_INT  56  // plain decimal — keep in sync with FIRMWARE_VERSION
#define DEVICE_NAME         "Brew370"

// OTA updates
#define OTA_MANIFEST_URL    "https://raw.githubusercontent.com/Korwiin/AN-BRU-370/main/ota/manifest.json"
