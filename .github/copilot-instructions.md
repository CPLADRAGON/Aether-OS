# AETHER_OS — Copilot Instructions

ESP32 room monitor (PlatformIO/Arduino) + Next.js 16 / React 19 dashboard backed by Supabase, with a Telegram bot deployed as a dashboard API route.

## Repo layout

- `firmware/` — PlatformIO project. All firmware lives in a single `src/main.cpp` (~1500 lines). Keep it single-file unless there's a strong reason to split.
- `dashboard/` — Next.js 16 app (App Router). Single-page dashboard in `src/app/page.tsx` (~930 lines) + `src/app/api/telegram/route.ts` webhook.
- `supabase_schema.sql` — 3 tables (`room_readings`, `device_sessions`, `device_logs`) with permissive anon RLS (INSERT + SELECT for all). No auth layer.
- `firmware/include/secrets.h` — gitignored; only a placeholder README is committed.

## Commands

### Firmware (from `firmware/`)
- Build: `pio run`
- Upload: `pio run -t upload`
- Monitor: `pio device monitor`
- Combined: `pio run -t upload && pio device monitor`
- Board: `esp32dev`, framework: `arduino` (see `platformio.ini`).

### Dashboard (from `dashboard/`)
- Dev: `npm run dev` (requires `.env.local` with Supabase creds)
- Build: `npm run build`
- Lint: `npm run lint`
- Start: `npm start`

There is no test runner configured in either project.

## Dashboard: Next.js 16 + React 19 (important)

**This is NOT the Next.js you know.** This version has breaking changes — APIs, conventions, and file structure may all differ from training data. Before writing dashboard code, read the relevant guide in `dashboard/node_modules/next/dist/docs/` and heed deprecation notices.

Additional conventions:
- **Tailwind v4** with CSS-first config inside `src/app/globals.css` (`@theme inline {}`). There is no `tailwind.config.js`.
- Icons use **Google Material Symbols** (not Font Awesome).
- Font: **Space Grotesk** loaded via Next.js font loader in `layout.tsx`.
- No external state library — all state is local `useState`/`useMemo` in `page.tsx`.
- Single Supabase client in `src/lib/supabase.ts`; import from there rather than instantiating new clients.
- Real-time uses Supabase `postgres_changes` on `room_readings` and `device_logs` INSERTs, maintaining a rolling window (1000 readings / 50 logs).
- Custom `DatePicker` (`src/components/DatePicker.tsx`) supports day/week/month/year modes — reuse it rather than adding a new date library.

## Firmware architecture

Single `main.cpp` divided into subsystems that share global state; changes must respect these boundaries:

- **Dual-core RTOS**: `uiTask` pinned to Core 0 (OLED render loop, ~150ms), `monitorTask` pinned to Core 1 (sensors, WiFi, Supabase). The Arduino `loop()` is intentionally empty — do not put work there.
- **Binary NVS (Preferences)**: All persistence uses packed C-structs, not JSON. Namespaces: `wifi_v2`, `config_v2`, `logs_v2`, `aether`, `aether_wifi`, `stats`. Structs include `WiFiSnapshot`, `DeviceConfig`, `LogQueue` (10-entry cyclic session buffer). Bump the namespace suffix if you change struct layout.
- **Memory-Link WiFi**: First connect saves BSSID + static IP to NVS; reconnect uses `WiFi.config()` + `WiFi.begin(bssid, channel)` for sub-500ms reconnect. Preserve this path when touching WiFi code.
- **Connect-on-demand**: WiFi is only initialized when a feature needs it (measure/weather/locate). Menu navigation must stay fully offline.
- **WiFi profile slots**: 5 credential slots in `aether_wifi` namespace with auto-cycling fallback; captive-portal AP is `AETHER_CONFIG`.
- **ISR button handling**: GPIO 33, CHANGE mode. Short press = menu nav, long press ≥1.2s = select. State lives in `IRAM_ATTR volatile` globals — keep ISR code minimal and don't call non-IRAM-safe APIs from it.
- **Measurement flow**: 5 samples × 2.5s (20s total), validated (NaN/range), averaged, POSTed to Supabase. Offline path queues into `LogQueue` for later flush.
- **Deep sleep**: Timer wake (5/15/30/60 min) or ext0 (button). Runs OLED shutdown animation and detaches LED pins to inputs before sleeping to minimize draw.

### State enums (grep before adding a new state)
- `SystemState`: `SS_MENU`, `SS_CONNECTING`, `SS_SCANNING`, `SS_SYNCING`, `SS_LOCATING`, `SS_CLOCK`, `SS_WEATHER`, `SS_SLEEPING`, `SS_STATS`, `SS_RESET`, `SS_PORTAL`, `SS_WIFI_MENU`
- `MenuPage`: `PAGE_MEASURE`, `PAGE_TIME`, `PAGE_WEATHER`, `PAGE_LOCATE`, `PAGE_LED`, `PAGE_INTERVAL`, `PAGE_STATS`, `PAGE_PORTAL`, `PAGE_RESET`, `PAGE_SLEEP`

## Dashboard analytics conventions

- **Comfort Index**: score derived from temp (ideal 24°C) and humidity (ideal 50%).
- **Presence detection**: heuristic on LDR + humidity deltas over a 30-min sliding window.
- **Apparent temperature**: heat-index calc surfaced in ECharts tooltip.
- **Efficiency Index**: measured against an 18s ideal sync baseline.
- **Charts**: ECharts 3-axis overlay (temp/humidity/light) with `markArea` for dark periods. Session Volatility bar chart flips amber when values >25s.
- **Dynamic background**: interpolates dark slate → warm brown by LDR value.
- **Timezone**: all data localized to SGT (UTC+8).

## Telegram bot (`dashboard/src/app/api/telegram/route.ts`)

Commands: `/start`, `/status`, `/stats`, `/report`. `/report` uses an inline keyboard for 24h/7d/30d/1y windows. Deployed as a Next.js route handler; Telegram webhook points here.

## Gotchas

- Do **not** commit `firmware/include/secrets.h` or dashboard `.env.local`.
- Supabase has no auth — add authentication before exposing real sensitive data.
- When adding NVS fields, migrate the namespace (`_v2` → `_v3`) rather than silently changing layout — old devices will read garbage otherwise.
