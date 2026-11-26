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
