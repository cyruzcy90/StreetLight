# Smart Street Light — All Files (copy/paste)

Everything is here in copy order. Each heading is the exact path to create.

---

## 1. `platformio.ini`

```ini
; PlatformIO Project Configuration
; Open this folder in Visual Studio Code with the PlatformIO IDE extension.

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
```

---

## 2. `include/config.h`

```cpp
#pragma once

// ---- Wi-Fi your ESP32 joins (must have internet access) --------------------
#define WIFI_SSID "PLDTHOMEFIBRAd63T"
#define WIFI_PASS "PLDTWIFIhYp9j"

// ---- Hosted website -------------------------------------------------------
// Filled in after the site is deployed.
#define SITE_URL "https://smart-street-light-0zepze-preview.freebuff.site"

// Shared secret: must match the DEVICE_KEY secret on the server.
#define DEVICE_KEY "f324a01376234444af538e503a73ae79"

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
```

---

## 3. `src/main.cpp`

```cpp
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
```

---

## 4. `website/src/worker.js`

```js
// Cloud relay for the ESP32 smart street light.
//
//   Browser -> POST /api/command       send ON/OFF/AUTO  (needs X-Device-Key)
//   Browser -> GET  /api/state         read live status  (public)
//   ESP32   -> POST /api/device/sync   report state + collect queued commands (needs X-Device-Key)
//
// Everything else is served from ./public by the assets binding.

const COMMAND_TTL_MS = 60 * 1000; // ignore commands the device never collected
const MAX_COMMANDS = 10;
const OFFLINE_AFTER_MS = 10 * 1000;

const PINS = [
  { gpioLed: 26, gpioIr: 32 },
  { gpioLed: 27, gpioIr: 33 },
  { gpioLed: 14, gpioIr: 25 },
];

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    try {
      switch (url.pathname) {
        case "/api/state":
          if (request.method !== "GET") return json({ error: "method_not_allowed" }, 405);
          return getState(env);
        case "/api/command":
          if (request.method !== "POST") return json({ error: "method_not_allowed" }, 405);
          return postCommand(request, env);
        case "/api/device/sync":
          if (request.method !== "POST") return json({ error: "method_not_allowed" }, 405);
          return deviceSync(request, env);
      }
    } catch (err) {
      return json({ error: "server_error", detail: String(err && err.message ? err.message : err) }, 500);
    }

    if (url.pathname.startsWith("/api/")) return json({ error: "not_found" }, 404);
    return env.ASSETS.fetch(request);
  },
};

// ---------------------------------------------------------------- handlers

async function getState(env) {
  await ensureSchema(env);

  const row = await env.DB.prepare(
    "SELECT uptime, stations, last_seen FROM device_state WHERE id = 1"
  ).first();

  if (!row) {
    return json({
      online: false,
      uptime: 0,
      lastSeen: 0,
      stations: PINS.map((p) => ({ on: false, manual: false, triggers: 0, ...p })),
    });
  }

  let stations = [];
  try {
    stations = JSON.parse(row.stations);
  } catch {
    stations = [];
  }

  return json({
    online: Date.now() - Number(row.last_seen) < OFFLINE_AFTER_MS,
    uptime: Number(row.uptime) || 0,
    lastSeen: Number(row.last_seen) || 0,
    stations,
  });
}

async function postCommand(request, env) {
  if (!env.DEVICE_KEY) return json({ error: "server_not_configured" }, 503);
  if (!keyOk(request, env)) return json({ error: "unauthorized" }, 401);

  let body;
  try {
    body = await request.json();
  } catch {
    return json({ error: "bad_json" }, 400);
  }

  const st = body.st === "all" ? "all" : String(Number(body.st));
  if (!["all", "0", "1", "2"].includes(st)) return json({ error: "bad_station" }, 400);

  const act = String(body.act);
  if (!["on", "off", "auto"].includes(act)) return json({ error: "bad_action" }, 400);

  await ensureSchema(env);

  const now = Date.now();
  await env.DB.batch([
    env.DB.prepare("DELETE FROM commands WHERE created_at <= ?").bind(now - COMMAND_TTL_MS),
    env.DB.prepare("INSERT INTO commands (station, action, created_at) VALUES (?, ?, ?)").bind(st, act, now),
  ]);

  return json({ ok: true, queued: { st, act } });
}

async function deviceSync(request, env) {
  if (!env.DEVICE_KEY) return json({ error: "server_not_configured" }, 503);
  if (!keyOk(request, env)) return json({ error: "unauthorized" }, 401);

  let body;
  try {
    body = await request.json();
  } catch {
    return json({ error: "bad_json" }, 400);
  }

  const stations = Array.isArray(body.stations)
    ? body.stations.slice(0, 3).map((s) => ({
        on: Boolean(s.on),
        manual: Boolean(s.manual),
        triggers: Number(s.triggers) || 0,
        gpioLed: Number(s.gpioLed) || 0,
        gpioIr: Number(s.gpioIr) || 0,
      }))
    : [];

  await ensureSchema(env);

  const now = Date.now();
  await env.DB.prepare(
    `INSERT INTO device_state (id, uptime, stations, last_seen) VALUES (1, ?, ?, ?)
     ON CONFLICT(id) DO UPDATE SET
       uptime = excluded.uptime,
       stations = excluded.stations,
       last_seen = excluded.last_seen`
  )
    .bind(Number(body.uptime) || 0, JSON.stringify(stations), now)
    .run();

  const { results } = await env.DB.prepare(
    "SELECT id, station, action FROM commands WHERE created_at > ? ORDER BY id ASC LIMIT ?"
  )
    .bind(now - COMMAND_TTL_MS, MAX_COMMANDS)
    .all();

  if (results.length) {
    const ids = results.map((r) => r.id);
    const placeholders = ids.map(() => "?").join(",");
    await env.DB.prepare(`DELETE FROM commands WHERE id IN (${placeholders})`).bind(...ids).run();
  }

  return json({ commands: results.map((r) => ({ st: r.station, act: r.action })) });
}

// ----------------------------------------------------------------- helpers

async function ensureSchema(env) {
  if (!env.__schemaReady) {
    // D1's exec() splits on newlines, so each statement stays on one line.
    env.__schemaReady = env.DB.batch([
      env.DB.prepare(
        "CREATE TABLE IF NOT EXISTS device_state (id INTEGER PRIMARY KEY CHECK (id = 1), uptime INTEGER NOT NULL DEFAULT 0, stations TEXT NOT NULL DEFAULT '[]', last_seen INTEGER NOT NULL DEFAULT 0)"
      ),
      env.DB.prepare(
        "CREATE TABLE IF NOT EXISTS commands (id INTEGER PRIMARY KEY AUTOINCREMENT, station TEXT NOT NULL, action TEXT NOT NULL, created_at INTEGER NOT NULL)"
      ),
    ]).catch((err) => {
      env.__schemaReady = null;
      throw err;
    });
  }
  await env.__schemaReady;
}

function keyOk(request, env) {
  const given = request.headers.get("x-device-key") || "";
  const expected = env.DEVICE_KEY || "";
  if (!expected || given.length !== expected.length) return false;

  let diff = 0;
  for (let i = 0; i < expected.length; i++) {
    diff |= given.charCodeAt(i) ^ expected.charCodeAt(i);
  }
  return diff === 0;
}

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store",
    },
  });
}
```

---

## 5. `website/public/index.html`

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Street Light OS</title>
  <link rel="stylesheet" href="/styles.css">
</head>
<body>

  <!-- Vertical Navigation Sidebar -->
  <aside class="sidebar">
    <div class="brand">
      <div class="brand-icon">⚡</div>
      <span>StreetLight OS</span>
    </div>
    <div class="nav-label">Menu</div>
    <div class="nav-list">
      <div class="nav-item active" id="nav-home" onclick="selectView('home')">
        <span>🏠 Home Overview</span>
      </div>
      <div class="nav-label">LED Stations</div>
      <div class="nav-item" id="nav-0" onclick="selectView(0)">
        <span>💡 Station 1</span>
        <span class="dot" id="dot-0"></span>
      </div>
      <div class="nav-item" id="nav-1" onclick="selectView(1)">
        <span>💡 Station 2</span>
        <span class="dot" id="dot-1"></span>
      </div>
      <div class="nav-item" id="nav-2" onclick="selectView(2)">
        <span>💡 Station 3</span>
        <span class="dot" id="dot-2"></span>
      </div>
    </div>
  </aside>

  <!-- Main Dashboard -->
  <main class="main">
    <div class="header">
      <div>
        <h1 id="page-title">Home Dashboard</h1>
        <p>Cloud connected • ESP32 via internet relay</p>
      </div>
      <div class="header-tools">
        <span class="conn" id="conn-badge">● CHECKING</span>
        <div class="key-box">
          <input id="key-input" type="password" placeholder="Device access key" autocomplete="off">
          <button class="btn-outline" onclick="saveKey()">Save</button>
        </div>
        <div class="global-actions" id="global-controls">
          <button class="btn-outline" onclick="sendCmd('all', 'on')">All ON</button>
          <button class="btn-outline" onclick="sendCmd('all', 'off')">All OFF</button>
          <button class="btn-auto" onclick="sendCmd('all', 'auto')">All AUTO</button>
        </div>
      </div>
    </div>

    <div class="notice" id="notice"></div>

    <!-- Home Summary Metrics -->
    <div class="metrics" id="home-metrics">
      <div class="metric-box">
        <div class="metric-title">Active Lights</div>
        <div class="metric-val" id="m-active">0 / 3</div>
      </div>
      <div class="metric-box">
        <div class="metric-title">Total IR Triggers</div>
        <div class="metric-val" id="m-triggers">0</div>
      </div>
      <div class="metric-box">
        <div class="metric-title">Device Uptime</div>
        <div class="metric-val" id="m-uptime">0s</div>
      </div>
      <div class="metric-box">
        <div class="metric-title">Last Contact</div>
        <div class="metric-val" id="m-seen">never</div>
      </div>
    </div>

    <!-- Dynamic Station Cards -->
    <div class="grid" id="cards-container"></div>
  </main>

  <script src="/app.js"></script>
</body>
</html>
```

---

## 6. `website/public/styles.css`

```css
:root {
  --bg-main: #0f172a;
  --bg-sidebar: #1e293b;
  --bg-card: #1e293b;
  --border: #334155;
  --text-main: #f8fafc;
  --text-muted: #94a3b8;
  --accent: #3b82f6;
  --green: #10b981;
  --red: #ef4444;
  --amber: #f59e0b;
}

* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
body { background: var(--bg-main); color: var(--text-main); display: flex; min-height: 100vh; }

/* Vertical Navigation Bar */
.sidebar {
  width: 240px;
  background: var(--bg-sidebar);
  border-right: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  padding: 24px 16px;
  flex-shrink: 0;
}
.brand {
  display: flex;
  align-items: center;
  gap: 12px;
  font-size: 1.15rem;
  font-weight: 700;
  padding-bottom: 24px;
  margin-bottom: 20px;
  border-bottom: 1px solid var(--border);
  color: #fff;
}
.brand-icon {
  width: 32px; height: 32px;
  background: var(--accent);
  border-radius: 8px;
  display: grid; place-items: center;
  font-weight: bold;
}
.nav-label {
  font-size: 0.72rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--text-muted);
  margin: 12px 0 8px 12px;
  font-weight: 600;
}
.nav-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 14px;
  margin-bottom: 6px;
  border-radius: 8px;
  color: var(--text-muted);
  cursor: pointer;
  transition: all 0.2s;
  font-weight: 500;
  font-size: 0.95rem;
}
.nav-item:hover { background: rgba(255,255,255,0.05); color: #fff; }
.nav-item.active { background: var(--accent); color: #fff; }

.dot { width: 8px; height: 8px; border-radius: 50%; background: #475569; transition: 0.3s; }
.dot.on { background: var(--green); box-shadow: 0 0 8px var(--green); }

/* Main Content Area */
.main { flex: 1; padding: 32px; max-width: 1100px; }
.header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  flex-wrap: wrap;
  gap: 16px;
  margin-bottom: 20px;
}
.header h1 { font-size: 1.6rem; font-weight: 700; }
.header p { color: var(--text-muted); font-size: 0.9rem; margin-top: 4px; }
.header-tools { display: flex; flex-direction: column; align-items: flex-end; gap: 10px; }

.conn {
  font-size: 0.75rem;
  font-weight: 700;
  letter-spacing: 0.05em;
  padding: 5px 12px;
  border-radius: 20px;
  border: 1px solid var(--border);
  color: var(--text-muted);
}
.conn.online { color: var(--green); border-color: rgba(16,185,129,0.4); background: rgba(16,185,129,0.12); }
.conn.offline { color: var(--red); border-color: rgba(239,68,68,0.4); background: rgba(239,68,68,0.12); }

.key-box { display: flex; gap: 6px; }
.key-box input {
  background: #0b1220;
  border: 1px solid var(--border);
  color: var(--text-main);
  border-radius: 8px;
  padding: 8px 10px;
  font-size: 0.8rem;
  width: 190px;
}

.notice {
  display: none;
  padding: 10px 14px;
  border-radius: 10px;
  font-size: 0.85rem;
  margin-bottom: 18px;
  border: 1px solid var(--border);
  color: var(--text-muted);
}
.notice.show { display: block; }
.notice.error { color: #fecaca; border-color: rgba(239,68,68,0.45); background: rgba(239,68,68,0.1); }
.notice.ok { color: #bbf7d0; border-color: rgba(16,185,129,0.4); background: rgba(16,185,129,0.1); }

/* Summary Metrics */
.metrics {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
  gap: 16px;
  margin-bottom: 28px;
}
.metric-box {
  background: var(--bg-card);
  border: 1px solid var(--border);
  padding: 18px;
  border-radius: 12px;
}
.metric-title { color: var(--text-muted); font-size: 0.8rem; text-transform: uppercase; letter-spacing: 0.05em; }
.metric-val { font-size: 1.5rem; font-weight: 700; margin-top: 6px; }

/* Global Controls */
.global-actions { display: flex; gap: 8px; flex-wrap: wrap; }

/* Station Cards Grid */
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(290px, 1fr));
  gap: 20px;
}
.card {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: 14px;
  padding: 22px;
  transition: transform 0.2s, border-color 0.2s;
}
.card.lit { border-color: rgba(16, 185, 129, 0.5); }
.card-top {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 18px;
  gap: 10px;
}
.station-title { font-size: 1.15rem; font-weight: 600; }
.badge {
  padding: 4px 10px;
  border-radius: 20px;
  font-size: 0.72rem;
  font-weight: 700;
  letter-spacing: 0.04em;
  white-space: nowrap;
}
.badge-on { background: rgba(16, 185, 129, 0.15); color: var(--green); border: 1px solid rgba(16, 185, 129, 0.3); }
.badge-off { background: rgba(148, 163, 184, 0.12); color: var(--text-muted); border: 1px solid var(--border); }

.info-row {
  display: flex;
  justify-content: space-between;
  padding: 10px 0;
  border-bottom: 1px solid rgba(255,255,255,0.06);
  font-size: 0.9rem;
  gap: 10px;
}
.info-row:last-of-type { border-bottom: none; margin-bottom: 18px; }
.info-label { color: var(--text-muted); }
.info-val { font-weight: 600; text-align: right; }

/* Buttons */
.btn-group { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
button {
  padding: 10px 12px;
  border: none;
  border-radius: 8px;
  font-weight: 600;
  font-size: 0.85rem;
  cursor: pointer;
  transition: opacity 0.2s, transform 0.1s;
  color: #fff;
}
button:hover { opacity: 0.9; }
button:active { transform: scale(0.97); }
.btn-on { background: var(--green); }
.btn-off { background: var(--red); }
.btn-auto { background: var(--accent); }
.btn-outline { background: transparent; border: 1px solid var(--border); color: var(--text-main); }
.btn-outline:hover { background: rgba(255,255,255,0.06); }

/* Responsive Mobile Sidebar */
@media (max-width: 768px) {
  body { flex-direction: column; }
  .sidebar { width: 100%; border-right: none; border-bottom: 1px solid var(--border); padding: 14px; }
  .brand { padding-bottom: 12px; margin-bottom: 10px; }
  .nav-label { display: none; }
  .nav-list { display: flex; gap: 6px; overflow-x: auto; }
  .nav-item { margin-bottom: 0; white-space: nowrap; flex: 1; justify-content: center; gap: 8px; }
  .main { padding: 20px 16px; }
  .header-tools { align-items: stretch; width: 100%; }
  .key-box input { width: 100%; }
}
```

---

## 7. `website/public/app.js`

```js
// Dashboard logic: polls the Worker API and sends commands to the ESP32.

const KEY_STORAGE = "streetlight.deviceKey";
const POLL_MS = 1500;

const FALLBACK_PINS = [
  { gpioLed: 26, gpioIr: 32 },
  { gpioLed: 27, gpioIr: 33 },
  { gpioLed: 14, gpioIr: 25 },
];

let currentView = "home";
let stateData = {
  online: false,
  uptime: 0,
  lastSeen: 0,
  stations: FALLBACK_PINS.map((p) => ({ on: false, manual: false, triggers: 0, ...p })),
};

function getKey() {
  return localStorage.getItem(KEY_STORAGE) || "";
}

function saveKey() {
  const value = document.getElementById("key-input").value.trim();
  if (!value) return setNotice("Enter the device access key first.", "error");
  localStorage.setItem(KEY_STORAGE, value);
  document.getElementById("key-input").value = "";
  setNotice("Access key saved on this device.", "ok");
}

function setNotice(text, kind) {
  const el = document.getElementById("notice");
  if (!text) {
    el.className = "notice";
    el.innerText = "";
    return;
  }
  el.className = "notice show " + (kind || "");
  el.innerText = text;
}

function selectView(view) {
  currentView = view;
  document.querySelectorAll(".nav-item").forEach((el) => el.classList.remove("active"));
  document.getElementById("nav-" + view).classList.add("active");

  const title = document.getElementById("page-title");
  const metrics = document.getElementById("home-metrics");
  const globals = document.getElementById("global-controls");

  if (view === "home") {
    title.innerText = "Home Dashboard";
    metrics.style.display = "grid";
    globals.style.display = "flex";
  } else {
    title.innerText = "Station " + (view + 1) + " Control";
    metrics.style.display = "none";
    globals.style.display = "none";
  }
  render();
}

async function sendCmd(st, action) {
  const key = getKey();
  if (!key) return setNotice("Enter the device access key to send commands.", "error");

  try {
    const res = await fetch("/api/command", {
      method: "POST",
      headers: { "content-type": "application/json", "x-device-key": key },
      body: JSON.stringify({ st, act: action }),
    });

    if (res.status === 401) return setNotice("Access key rejected by the server.", "error");
    if (res.status === 503) return setNotice("Server is missing its device key configuration.", "error");
    if (!res.ok) return setNotice("Command failed (HTTP " + res.status + ").", "error");

    setNotice(
      stateData.online
        ? "Command queued — the ESP32 applies it within a second."
        : "Command queued, but the ESP32 is offline. It will apply when it reconnects (within 60s).",
      stateData.online ? "ok" : "error"
    );
    fetchStatus();
  } catch {
    setNotice("Could not reach the server.", "error");
  }
}

function formatUptime(sec) {
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  if (h > 0) return `${h}h ${m}m`;
  return m > 0 ? `${m}m ${s}s` : `${s}s`;
}

function formatAgo(ts) {
  if (!ts) return "never";
  const sec = Math.max(0, Math.round((Date.now() - ts) / 1000));
  return sec < 2 ? "just now" : sec + "s ago";
}

function render() {
  let activeCount = 0;
  let totalTriggers = 0;

  stateData.stations.forEach((s, idx) => {
    if (s.on) activeCount++;
    totalTriggers += s.triggers;
    const dot = document.getElementById("dot-" + idx);
    if (dot) dot.className = s.on ? "dot on" : "dot";
  });

  document.getElementById("m-active").innerText = `${activeCount} / 3`;
  document.getElementById("m-triggers").innerText = totalTriggers;
  document.getElementById("m-uptime").innerText = formatUptime(stateData.uptime);
  document.getElementById("m-seen").innerText = formatAgo(stateData.lastSeen);

  const badge = document.getElementById("conn-badge");
  badge.className = "conn " + (stateData.online ? "online" : "offline");
  badge.innerText = stateData.online ? "● DEVICE ONLINE" : "● DEVICE OFFLINE";

  const container = document.getElementById("cards-container");
  container.innerHTML = "";

  stateData.stations.forEach((s, idx) => {
    if (currentView !== "home" && currentView !== idx) return;

    const card = document.createElement("div");
    card.className = "card" + (s.on ? " lit" : "");
    card.innerHTML = `
      <div class="card-top">
        <span class="station-title">Station ${idx + 1}</span>
        <span class="badge ${s.on ? "badge-on" : "badge-off"}">${s.on ? "● ONLINE / ON" : "○ STANDBY / OFF"}</span>
      </div>
      <div class="info-row">
        <span class="info-label">Operation Mode</span>
        <span class="info-val" style="color:${s.manual ? "#f59e0b" : "#38bdf8"}">
          ${s.manual ? "Manual Override" : "Auto (IR Sensor)"}
        </span>
      </div>
      <div class="info-row">
        <span class="info-label">Motion Triggers</span>
        <span class="info-val">${s.triggers}</span>
      </div>
      <div class="info-row">
        <span class="info-label">Hardware Pins</span>
        <span class="info-val">LED: GPIO ${s.gpioLed} | IR: GPIO ${s.gpioIr}</span>
      </div>
      <div class="btn-group">
        <button class="btn-on" onclick="sendCmd(${idx}, 'on')">ON</button>
        <button class="btn-off" onclick="sendCmd(${idx}, 'off')">OFF</button>
        <button class="btn-auto" onclick="sendCmd(${idx}, 'auto')">AUTO</button>
      </div>
    `;
    container.appendChild(card);
  });
}

async function fetchStatus() {
  try {
    const res = await fetch("/api/state", { cache: "no-store" });
    if (!res.ok) throw new Error("HTTP " + res.status);
    stateData = await res.json();
    render();
  } catch {
    const badge = document.getElementById("conn-badge");
    badge.className = "conn offline";
    badge.innerText = "● SERVER UNREACHABLE";
  }
}

render();
fetchStatus();
setInterval(fetchStatus, POLL_MS);
```

---

## 8. `website/deploy.json`

```json
{
  "entrypoint": "worker.js",
  "compatibilityDate": "2025-11-01",
  "modules": [
    { "name": "worker.js", "type": "esm", "path": "./src/worker.js" }
  ],
  "assetsDirectory": "./public"
}
```

---

## 9. `website/wrangler.jsonc`

```jsonc
// Local development only. Production values are injected by the hosting platform.
{
  "name": "streetlight-web",
  "main": "src/worker.js",
  "compatibility_date": "2025-11-01",
  "assets": {
    "directory": "./public",
    "binding": "ASSETS"
  },
  "d1_databases": [
    {
      "binding": "DB",
      "database_name": "streetlight",
      "database_id": "00000000-0000-0000-0000-000000000000"
    }
  ],
  "vars": {
    "DEVICE_KEY": "dev-key-change-me"
  }
}
```

---

## 10. `website/package.json`

```json
{
  "name": "streetlight-web",
  "private": true,
  "scripts": {
    "dev": "wrangler dev",
    "deploy": "wrangler deploy"
  },
  "devDependencies": {
    "wrangler": "^4.0.0"
  }
}
```

---

## Quick facts

- **Live site:** https://smart-street-light-0zepze-preview.freebuff.site
- **Access key (paste into dashboard):** `f324a01376234444af538e503a73ae79`
- **Pins:** IR 32 / 33 / 25, LED 26 / 27 / 14, `LIGHT_DURATION` 5000 ms, IR active LOW.
- The access key in `config.h` must match the `DEVICE_KEY` secret on the server.
