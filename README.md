# Smart Street Light — ESP32 + hosted dashboard

Three IR-triggered LED stations, controlled from a website over the internet.

## What's in this folder

| Path | What it is |
|---|---|
| `platformio.ini` | Board + library config (ESP32 Dev Module, ArduinoJson) |
| `include/config.h` | **Your Wi-Fi name/password, site URL and device key** |
| `src/main.cpp` | Firmware: IR sensors, 5 s light timer, cloud sync |
| `website/` | The hosted dashboard (already deployed, you don't need to touch it) |
| `COPY-ALL-FILES.md` | Every file as plain text, for copy/paste |

Pins: IR on GPIO **32, 33, 25** · LEDs on GPIO **26, 27, 14** · light stays on **5000 ms**.

---

## 1. Install (once)

1. **Visual Studio Code** — https://code.visualstudio.com
2. In VS Code open the Extensions panel (`Ctrl+Shift+X`), search **PlatformIO IDE**, click **Install**.
3. Restart VS Code when it asks.

You do **not** need the Arduino IDE. PlatformIO downloads the ESP32 toolchain by itself.

---

## 2. Open the project

**File → Open Folder…** → choose this `smart-street-light` folder → **Select Folder**.

A blue PlatformIO bar appears at the bottom of the window. If it doesn't, click the alien/ant icon in the left sidebar and pick **Open**.

---

## 3. Check your settings

Open `include/config.h` and confirm:

```cpp
#define WIFI_SSID "PLDTHOMEFIBRAd63T"   // your Wi-Fi — must have internet
#define WIFI_PASS "PLDTWIFIhYp9j"
#define SITE_URL  "https://smart-street-light-0zepze-preview.freebuff.site"
#define DEVICE_KEY "f324a01376234444af538e503a73ae79"
```

The ESP32 joins your Wi-Fi as a client, so 2.4 GHz only (not 5 GHz) and the network needs internet.

---

## 4. Build

Click the **✓ (checkmark)** icon in the blue bottom bar.

- The very first build downloads the toolchain — expect **5–15 minutes** and a lot of text.
- Later builds take seconds.
- Success looks like: `SUCCESS | RAM: 14.9% | Flash: 77.8%`.

---

## 5. Upload to the ESP32

1. Plug the board in with a **data** USB cable (not charge-only).
2. If the port isn't auto-detected: bottom bar **→ (arrow) → Upload**; if it fails, install the CP210x or CH340 USB driver for your board.
3. Click the **→ (right arrow)** icon. Wait for `SUCCESS`.
4. Click the **🔌 (plug)** icon to open the **Serial Monitor**. It should already be at **115200 baud**.

Expected output:

```
Connecting to Wi-Fi: PLDTHOMEFIBRAd63T
Connected. IP: 192.168.x.x
--- System Ready ---
Reporting to: https://smart-street-light-0zepze-preview.freebuff.site
Station 1 Triggered!
```

If it prints `Wi-Fi not connected, retrying on next sync.`, the SSID/password or the 2.4 GHz band is the problem.

---

## 6. Use the dashboard

Open https://smart-street-light-0zepze-preview.freebuff.site

1. Paste the access key `f324a01376234444af538e503a73ae79` into **Device access key** → **Save**.
2. Within ~1 second the badge turns **● DEVICE ONLINE**.
3. **ON / OFF / AUTO** per station, or **All ON / All OFF / All AUTO**.
   - ON/OFF = manual override, sensor ignored.
   - AUTO = back to IR sensor control.

The ESP32 reports in once per second, so a button press lands in about a second. If the badge is offline the board isn't reaching Wi-Fi or the internet.

---

## Using the Arduino IDE instead (optional)

The same firmware exists as one plain Arduino sketch: `arduino/smart_street_light/smart_street_light.ino`. No PlatformIO needed.

1. Install **Arduino IDE 2.x** — https://www.arduino.cc/en/software
2. **File → Preferences → Additional boards manager URLs**, paste:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. **Boards Manager** (left sidebar) → search `esp32` → install **esp32 by Espressif Systems** (large download, 5–20 min).

   That is the only install. The sketch needs **no libraries** — skip Library Manager entirely; `WiFi`, `HTTPClient` and `WiFiClientSecure` already come with the ESP32 package.
4. **File → Open** → `arduino/smart_street_light/smart_street_light.ino`. Your Wi-Fi, `SITE_URL` and `DEVICE_KEY` are at the top of that file.
5. **Tools → Board → esp32 → ESP32 Dev Module** and **Tools → Port →** your COM port.
6. Click **→ Upload**. Then **Tools → Serial Monitor**, baud **115200**.

Keep the folder name `smart_street_light` — the Arduino IDE needs the `.ino` file to match its folder name. The `website/` folder is unaffected either way.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `→` upload fails, "no port found" | Different cable/port; install CP210x or CH340 driver |
| `Connecting…` forever, then timeout | Wrong Wi-Fi password, or 5 GHz network |
| Badge stays **DEVICE OFFLINE** | Check Serial Monitor — it prints the HTTP code when the server rejects it |
| Key rejected in browser | Typo, or `DEVICE_KEY` doesn't match the server secret |
| Lights behave backwards | Set `IR_ACTIVE_HIGH = true` in `config.h` (for PIR sensors) |

---

## Optional: run the website locally

```bash
cd website
npm install
npx wrangler dev
```

Requires Node.js. The deployed site already works, so this is only for editing the dashboard.
