/*
  Smart Street Light — Arduino IDE version (single file)

  NO EXTRA LIBRARIES NEEDED. Everything here ships with the ESP32 board
  package itself, so there is nothing to install from Library Manager.

  Board:  Tools -> Board -> esp32 -> "ESP32 Dev Module"
  Port:   Tools -> Port -> your COM port
  Speed:  Tools -> Upload Speed -> 921600
  Serial: Tools -> Serial Monitor -> 115200 baud

  Board support: esp32 by Espressif Systems (Boards Manager)
  -> Boards Manager needs this URL in File -> Preferences ->
     Additional boards manager URLs:
     https://espressif.github.io/arduino-esp32/package_esp32_index.json

  Keep this file inside a folder with the same name: smart_street_light/
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ============================ SETTINGS =====================================

// ---- Wi-Fi your ESP32 joins (must be 2.4 GHz with internet access) --------
#define WIFI_SSID "PLDTHOMEFIBRAd63T"
#define WIFI_PASS "PLDTWIFIhYp9j"

// ---- Hosted website ------------------------------------------------------
#define SITE_URL "https://smart-street-light-0zepze-preview.freebuff.site"

// Shared secret: must match the DEVICE_KEY secret on the server.
#define DEVICE_KEY "f324a01376234444af538e503a73ae79"

// ---- Hardware ------------------------------------------------------------
// Set to false for standard IR obstacle sensors (active LOW)
// Set to true if using PIR motion sensors (active HIGH)
const bool IR_ACTIVE_HIGH = false;

const int IR_PINS[3] = {32, 33, 25};
const int LED_PINS[3] = {26, 27, 14};

// How often the ESP32 talks to the website (ms)
const unsigned long SYNC_INTERVAL = 1000;
// How long a light stays on after motion (ms)
const unsigned long LIGHT_DURATION = 5000;

// ============================== CODE =======================================

bool ledState[3] = {false, false, false};
bool manualOverride[3] = {false, false, false};
unsigned long ledTurnOnTime[3] = {0, 0, 0};
unsigned long triggerCount[3] = {0, 0, 0};

unsigned long lastSync = 0;
bool serverOnline = false;

// --------------------------------------------------------------------- Wi-Fi
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi not connected, retrying on next sync.");
  }
}

// ------------------------------------------------------------------- actions
void applyAction(int st, const String &act) {
  if (act == "on") {
    manualOverride[st] = true;
    ledState[st] = true;
    digitalWrite(LED_PINS[st], HIGH);
  } else if (act == "off") {
    manualOverride[st] = true;
    ledState[st] = false;
    digitalWrite(LED_PINS[st], LOW);
  } else if (act == "auto") {
    manualOverride[st] = false;
    ledState[st] = false;
    digitalWrite(LED_PINS[st], LOW);
  } else {
    return;
  }

  Serial.print("Command: station ");
  Serial.print(st + 1);
  Serial.print(" -> ");
  Serial.println(act);
}

// --------------------------------------------------------------- sensor logic
void updateSensors() {
  unsigned long now = millis();

  for (int i = 0; i < 3; i++) {
    if (manualOverride[i]) continue;

    bool motion = (digitalRead(IR_PINS[i]) == (IR_ACTIVE_HIGH ? HIGH : LOW));

    if (motion) {
      if (!ledState[i]) {
        triggerCount[i]++;
        Serial.print("Station ");
        Serial.print(i + 1);
        Serial.println(" Triggered!");
      }
      ledState[i] = true;
      ledTurnOnTime[i] = now;
      digitalWrite(LED_PINS[i], HIGH);
    }

    if (ledState[i] && (now - ledTurnOnTime[i] >= LIGHT_DURATION)) {
      ledState[i] = false;
      digitalWrite(LED_PINS[i], LOW);
      Serial.print("Station ");
      Serial.print(i + 1);
      Serial.println(" Timeout OFF.");
    }
  }
}

// -------------------------------------------------- read the queued commands
// The server answers like:  {"commands":[{"st":"0","act":"on"}]}
// This scans that text for the st/act pairs, so no JSON library is required.
void handleCommands(const String &payload) {
  int from = 0;

  while (true) {
    int s = payload.indexOf("\"st\":\"", from);
    if (s < 0) break;

    int sEnd = payload.indexOf('"', s + 6);
    if (sEnd < 0) break;

    int a = payload.indexOf("\"act\":\"", sEnd);
    if (a < 0) break;

    int aEnd = payload.indexOf('"', a + 7);
    if (aEnd < 0) break;

    String st = payload.substring(s + 6, sEnd);
    String act = payload.substring(a + 7, aEnd);

    if (st == "all") {
      for (int i = 0; i < 3; i++) applyAction(i, act);
    } else {
      int idx = st.toInt();
      if (idx >= 0 && idx < 3) applyAction(idx, act);
    }

    from = aEnd + 1;
  }
}

// ------------------------------------------------------------ cloud sync call
// Reports current state and collects any commands the website queued up.
void syncWithServer() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return;
  }

  String body = "{\"uptime\":" + String(millis() / 1000) + ",\"stations\":[";
  for (int i = 0; i < 3; i++) {
    body += "{\"on\":" + String(ledState[i] ? "true" : "false");
    body += ",\"manual\":" + String(manualOverride[i] ? "true" : "false");
    body += ",\"triggers\":" + String(triggerCount[i]);
    body += ",\"gpioLed\":" + String(LED_PINS[i]);
    body += ",\"gpioIr\":" + String(IR_PINS[i]) + "}";
    if (i < 2) body += ",";
  }
  body += "]}";

  WiFiClientSecure client;
  client.setInsecure(); // school/demo setup: no certificate pinning
  HTTPClient http;
  http.setTimeout(6000);
  http.setReuse(true);

  String url = String(SITE_URL) + "/api/device/sync";
  if (!http.begin(client, url)) return;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);

  int code = http.POST(body);
  if (code == 200) {
    serverOnline = true;
    handleCommands(http.getString());
  } else {
    serverOnline = false;
    if (code > 0) {
      Serial.print("Server responded with ");
      Serial.println(code);
    }
  }

  http.end();
}

// ---------------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  delay(200);

  for (int i = 0; i < 3; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    pinMode(IR_PINS[i], INPUT);

    // Startup wiring check
    digitalWrite(LED_PINS[i], HIGH);
    delay(250);
    digitalWrite(LED_PINS[i], LOW);
  }

  connectWiFi();
  Serial.println("\n--- System Ready ---");
  Serial.print("Reporting to: ");
  Serial.println(SITE_URL);
}

void loop() {
  updateSensors();

  unsigned long now = millis();
  if (now - lastSync >= SYNC_INTERVAL) {
    lastSync = now;
    syncWithServer();
  }
}
