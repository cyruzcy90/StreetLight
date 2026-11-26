#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "config.h"

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
    String payload = http.getString();

    JsonDocument doc;
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      for (JsonObject cmd : doc["commands"].as<JsonArray>()) {
        String st = cmd["st"].as<String>();
        String act = cmd["act"].as<String>();

        if (st == "all") {
          for (int i = 0; i < 3; i++) applyAction(i, act);
        } else {
          int idx = st.toInt();
          if (idx >= 0 && idx < 3) applyAction(idx, act);
        }
      }
    }
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
