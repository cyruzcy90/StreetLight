#pragma once

// ---- Wi-Fi your ESP32 joins (must have internet access) --------------------
#define WIFI_SSID "PLDTHOMEFIBRAd63T"
#define WIFI_PASS "PLDTWIFIhYp9j"

// ---- Hosted website -------------------------------------------------------
// Filled in after the site is deployed.
#define SITE_URL "https://smart-street-light-0zepze-preview.freebuff.site"

// Shared secret: must match the DEVICE_KEY secret on the server.
#define DEVICE_KEY "pass"

// ---- Hardware -------------------------------------------------------------
// Set to false for standard IR obstacle sensors (active LOW)
// Set to true if using PIR motion sensors (active HIGH)
const bool IR_ACTIVE_HIGH = false;

const int IR_PINS[3] = {32, 33, 25};
const int LED_PINS[3] = {26, 27, 14};

// How often the ESP32 talks to the website (ms)
const unsigned long SYNC_INTERVAL = 1000;
// How long a light stays on after motion (ms)
const unsigned long LIGHT_DURATION = 5000;
