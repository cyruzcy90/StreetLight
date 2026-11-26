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
