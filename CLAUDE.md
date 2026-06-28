# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AETHER_OS — an ESP32-based room monitor with a Supabase-backed "Digital Twin" dashboard and Telegram bot integration. The ESP32 uses a dual-core architecture (Core 0 = UI/OLED, Core 1 = telemetry/networking) with binary NVS structs for persistent storage and fast-reconnect WiFi.

## Repo Structure

```
├── firmware/                    # ESP32 firmware (PlatformIO / Arduino)
│   ├── include/secrets.h        # WiFi/Supabase credentials (gitignored)
│   ├── src/main.cpp             # Single-file firmware (~1500 lines)
│   ├── test/                    # Tests
│   └── platformio.ini           # Board: esp32dev, framework: arduino
├── dashboard/                   # Next.js 16 + React 19 dashboard
│   ├── src/
│   │   ├── app/
│   │   │   ├── page.tsx         # Single-page dashboard (~930 lines)
│   │   │   ├── layout.tsx       # Root layout with Space Grotesk font
│   │   │   ├── globals.css      # Tailwind v4 + glassmorphism styles
│   │   │   └── api/telegram/route.ts  # Telegram bot webhook handler
│   │   ├── components/
│   │   │   └── DatePicker.tsx   # Custom date picker (day/week/month/year modes)
│   │   └── lib/
│   │       └── supabase.ts      # Single supabase client instance
│   ├── package.json
│   ├── tsconfig.json
│   └── vercel.json              # Vercel deploy config (framework: nextjs)
├── supabase_schema.sql          # 3 tables: room_readings, device_sessions, device_logs
├── DEPLOYMENT.md                # Hardware build + deployment guide
├── ANALYSIS.md                  # Architecture analysis and roadmap
└── docs/
    ├── pics/                    # Hardware and UI screenshots
    └── specs/                   # Design documents
```

## Supabase Schema

Three tables in `supabase_schema.sql` — all have anonymous RLS policies for INSERT and SELECT:

- **room_readings** — temperature, humidity, ldr_value, accel_total, battery_v, trigger_source, device_id
- **device_sessions** — start_time (Unix epoch), duration (seconds), boot_count, measure_count, sleep_interval, device_id
- **device_logs** — message, level (DEBUG/INFO/WARN/ERROR), device_id

## Key Commands

### Firmware (PlatformIO, inside `firmware/`)
```bash
# Build
pio run

# Upload to ESP32
pio run -t upload

# Serial monitor
pio device monitor

# Build + upload + monitor
pio run -t upload && pio device monitor
```

### Dashboard (inside `dashboard/`)
```bash
# Development (requires .env.local with Supabase creds)
npm run dev

# Production build
npm run build

# Lint
npm run lint

# Start production server
npm start
```

### Root
```bash
# Install all dashboard dependencies
cd dashboard && npm install
```

## Firmware Architecture

All code is in a single `firmware/src/main.cpp` with these major systems:

- **Dual-core RTOS**: `uiTask` pinned to Core 0 (OLED rendering at 150ms intervals), `monitorTask` pinned to Core 1 (sensors, WiFi, Supabase). Main `loop()` is empty.
- **Binary NVS (Preferences)**: `WiFiSnapshot` (BSSID/IP for fast reconnect), `DeviceConfig` (sleep interval, LED state, location), `LogQueue` (10-entry cyclic session buffer). Stored in NVS namespaces `wifi_v2`, `config_v2`, `logs_v2`, `aether`, `aether_wifi`, `stats`.
- **ISR Button Handling**: `handleButtonInterrupt()` on GPIO 33 (CHANGE mode). Distinguishes short press (menu navigation) from long press ≥1.2s (selection). Stores state in `IRAM_ATTR` volatile globals.
- **Memory-Link WiFi**: On first connect, saves BSSID + static IP config to NVS. On reconnect, uses `WiFi.config()` with saved IP and `WiFi.begin()` with BSSID/channel — sub-500ms reconnect.
- **Connect-on-Demand**: WiFi only initialized when a function needs it (measure, weather, locate). Menu navigation is offline.
- **WiFi Profile Slots**: 5 credential slots in `aether_wifi` namespace. Auto-cycling fallback. Captive portal AP (`AETHER_CONFIG`) for provisioning.
- **Measurement Flow**: 5 samples (2.5s each, 20s total), validated against NaN/range thresholds, averaged, posted to Supabase. Falls back to queuing session logs if offline.
- **Deep Sleep**: Timer wake (5/15/30/60 min) or ext0 wake (button). OLED shutdown animation, LED pins detached to inputs for current draw.

### System States (enum `SystemState`)
`SS_MENU`, `SS_CONNECTING`, `SS_SCANNING`, `SS_SYNCING`, `SS_LOCATING`, `SS_CLOCK`, `SS_WEATHER`, `SS_SLEEPING`, `SS_STATS`, `SS_RESET`, `SS_PORTAL`, `SS_WIFI_MENU`

### Menu Pages (enum `MenuPage`)
`PAGE_MEASURE`, `PAGE_TIME`, `PAGE_WEATHER`, `PAGE_LOCATE`, `PAGE_LED`, `PAGE_INTERVAL`, `PAGE_STATS`, `PAGE_PORTAL`, `PAGE_RESET`, `PAGE_SLEEP`

## Dashboard Architecture

Single-page Next.js app with 3 tabs: Dashboard (default), Sensors (unused), Power (runtime analytics).

### State Management
- No external state library — all local `useState` and `useMemo` in `page.tsx`
- Custom `DatePicker` component with 4 modes: day/week/month/year
- Data fetched via Supabase JS SDK with `gte`/`lte` time range filters

### Real-time
Supabase Realtime subscription (`postgres_changes` on `room_readings` INSERT and `device_logs` INSERT) — maintains a rolling window of 1000 readings and 50 logs.

### Key Features
- **Comfort Index**: Simple score based on temperature (ideal 24°C) and humidity (ideal 50%)
- **Presence Detection**: Heuristic using LDR + humidity deltas over 30-minute sliding window
- **Apparent Temperature**: Heat index calculation displayed in chart tooltip
- **ECharts Visualization**: 3-axis overlay (temp/humidity/light) with `markArea` for dark periods
- **KPI Cards**: Accumulated Uptime, Mean Session Duration, Device Lifecycle (boot/sync), Efficiency Index (vs 18s ideal)
- **Session Volatility Index**: Bar chart with color thresholds (>25s = amber)
- **Dynamic Background**: Interpolates between dark slate (low light) and warm brown (high light)

### Telegram Bot
- `/start`, `/status`, `/stats`, `/report` commands
- Inline keyboard for report timeframe selection (24h/7d/30d/1y)
- Deployed as Next.js API route, configured as Telegram webhook

## Common Gotchas

- **Firmware is single-file** — `main.cpp` is ~1530 lines. Keep it that way unless there's a strong reason to split.
- **Dashboard uses Next.js 16 + React 19** — check for API changes if upgrading. The AGENTS.md in dashboard/ warns about breaking changes from earlier Next.js versions.
- **Tailwind v4** — uses CSS-first config in `globals.css` with `@theme inline {}`, not `tailwind.config.js`.
- **Fonts**: Google Material Symbols (not Font Awesome) for icons. Space Grotesk font via Next.js font loader.
- **No auth** — Supabase uses anon key with permissive RLS policies (INSERT + SELECT for all). Add authentication before deploying to production with sensitive data.
- **Secrets file** — `firmware/include/secrets.h` is gitignored. A placeholder `README` exists instead.
