# Aether-OS Feature Expansion Plan
## Features: Alarm Timer · Offline Queue · Calendar Heatmap · Compare Mode · PWA

---

## 1. Alarm / Countdown Timer (firmware)

**What**: A dedicated TIMER screen reachable from the main menu. User cycles
through preset durations (5 / 10 / 15 / 25 / 30 min) via short press, starts/
stops via long press. Timer counts down across deep sleep using `RTC_DATA_ATTR`
state (the RTC clock survives deep sleep, so elapsed time is simply
`millis_at_sleep + sleep_duration - start_time` on wake). On expiry, LED flashes
an attention pattern and OLED shows a TIMER DONE screen until dismissed.

**New items**:
- `RTC_DATA_ATTR` globals: `timerActive`, `timerEndEpoch` (UTC seconds)
- `showTimerPage()` — the page controller
- `drawTimerScreen()` — shows countdown HH:MM:SS + a progress arc/bar
- `drawTimerDone()` — expiry notification screen
- `PAGE_TIMER` added to `MenuPage` enum (after `PAGE_TREND`)
- On each wake from deep sleep, check `timerActive && time_now >= timerEndEpoch` before entering the normal menu loop; if true, show the done screen.

**Design notes**: No new hardware. Uses `g_menuOwnedByPage` pattern like every
other page. LED flashing uses existing `setLED()`. Long-press exits same as other
pages. Timer persists across sleep cycles via `RTC_DATA_ATTR` and NTP-derived UTC
time.

---

## 2. Offline Reading Queue (firmware)

**What**: Currently when WiFi fails after a measurement (line ~1602 in
`runMeasurementFlow()`), the averaged sensor data is silently discarded — the user
spent 20 seconds sampling and got nothing synced. Instead, queue the reading
locally (up to 20 entries) and flush it on the next successful upload.

**New items**:
- `OfflineReading` struct (packed): `float temp, humidity; uint16_t ldrRaw;
  uint16_t lux; float accel; uint32_t timestamp;`
- `OfflineQueue` struct: `version=1`, `count`, `OfflineReading entries[20]`
- NVS namespace `offline_v1`
- `appendOfflineReading(...)` — queues an entry, FIFO-overwrites oldest at cap
- `flushOfflineQueue(HTTPClient&, WiFiClientSecure&)` — sends queued entries as a
  JSON array to `room_readings` REST endpoint; clears on HTTP 2xx
- Called from `runMeasurementFlow()`: WiFi fail path saves to queue instead of
  returning empty; WiFi success path calls flush BEFORE the current reading upload
- NVS namespace bump (`offline_v1`) is independent of existing `logs_v2` /
  `trend_v2` namespaces — no migration needed

---

## 3. Calendar Heatmap (dashboard)

**What**: A 3-month calendar grid showing daily average comfort score or
temperature, styled like a GitHub contribution graph. Each cell is coloured on a
5-level scale from cool/dark to warm/bright. Added below the Trend Analysis card.

**New component**: `CalendarHeatmap.tsx`
- Props: `readings: Reading[]` (already fetched by the parent for the day/week/
  month/year view)
- For a month/year timeframe: derives daily averages client-side from the existing
  `readings` array (no extra query for common time ranges)
- For the day/week timeframe: shows a small "recent 30 days" window regardless
  (always useful; doesn't change with the timeframe selector)
- Color scale: `#1f1f23` (no data) → indigo shades → `#818cf8` (best) mapped to
  the comfort score

**Integration**: New section in `DashboardView.tsx` between the Sensor Details
stat chips and the Trend Analysis card.

---

## 4. Compare Mode (dashboard)

**What**: A toggle button on the Trend Analysis card header. When active, fetches
the equivalent prior period (yesterday for "day", last week for "week", etc.) and
overlays it as dashed lines behind the solid current-period lines.

**Changes**:
- New `useCompare` state in `page.tsx` (toggle button)
- When `useCompare` is true: second Supabase query for `[prevStart, prevEnd]` in
  the existing `doFetch` effect, stored in `prevReadings: Reading[]`
- In `TrendChart.tsx`: accept optional `prevReadings` prop; when present, add 3
  extra series with `lineStyle: { type: 'dashed', opacity: 0.4 }` sharing the
  same y-axes as the main series. Previous period data is time-shifted to align
  with the current period for visual comparison.
- Toggle button: a small icon button ("⧉ Compare") in the Trend Analysis header
  row alongside the period navigation.

---

## 5. PWA Support (dashboard)

**What**: Makes the dashboard installable as a home-screen app on iOS/Android.

**Changes**:
- `app/manifest.json` with `name`, `short_name`, `icons`, `display: standalone`,
  `background_color`, `theme_color`
- `app/layout.tsx`: add `<link rel="manifest">` and `<meta theme-color>` in the
  `<head>` (via Next.js `metadata` export or explicit `<head>` tags)
- `public/`: add icon files (192×192 and 512×512 PNG; can be a simple generated
  SVG/PNG with the existing Aether aesthetic)
- `next.config.js`: optionally add `next-pwa` for a service worker that caches
  the shell offline (static assets only; API calls still need network)
- No backend changes needed; `vercel.json` already handles the deployment

---

## Execution order

| # | Feature | Effort | Dependency |
|---|---|---|---|
| 5 | PWA | XS | None |
| 2 | Offline queue | S | None (firmware-only) |
| 1 | Alarm timer | M | None (firmware-only) |
| 3 | Calendar heatmap | M | None (dashboard-only) |
| 4 | Compare mode | M | None (dashboard-only) |

PWA first (trivial). Then offline queue (small firmware, high user value). Then
the remaining three in parallel or in any order.
