# Aether-OS Feature Expansion Plan
## Features: PWA · Offline Queue · Alarm Timer · Calendar Heatmap · Compare Mode
## + Immediate UI Fixes (progress bar, weather empty)

---

## Immediate fixes (small, do first)

### Fix A — Time Detail: progress bar always appears 100% full
Root cause (verified): the current code draws three full-width white
`dm::drawHLine` calls first (filling the entire bar white), then
`dm::drawFilledRect` for the filled portion — also white. Both portions
are white pixels. The unfilled section looks filled.

Fix: use `dm::drawRect()` (outline frame only, interior stays black) then
`dm::drawFilledRect` for just the filled interior pixels.

### Fix B — Weather main screen: add feels-like temp + tap indicator
Condition label is already in the detail subpage. Better additions:
1. **Feels-like temperature** — extracted from `doc["main"]["feels_like"]`
   (same API call). Show as "FEELS 29.2C" in FONT_SMALL, centred below
   the temperature.
2. **">" tap indicator** bottom-right corner — guides user to detail subpage
   (same affordance as Room screen).

`drawWeatherScreen()` gains a `float feelsLike` parameter. Temperature
shifts up from y=19 to y=15. Feels-like at y=37, tap indicator at y=41.

---

## 1. PWA Support (dashboard)

**Files:**
- `dashboard/public/manifest.json` — `name`, `short_name`, `display:
  standalone`, `theme_color: #0d0d0f`, `background_color: #0d0d0f`, icons
- `dashboard/public/icon-192.png`, `icon-512.png` — simple generated SVG
  converted to PNG (A glyph + dark background, Aether brand colours)
- `dashboard/src/app/layout.tsx` — add `<link rel="manifest">` inside
  `<head>` and `<meta name="theme-color">`, plus Apple PWA meta tags
  (`apple-mobile-web-app-capable`, `apple-touch-icon`)
- No service worker needed for v1 (just installability); can add caching
  later via `next-pwa` if desired

---

## 2. Offline Reading Queue (firmware)

**What:** When `ensureWiFi()` fails after a measurement, currently the
averaged sensor data is silently discarded. Queue it locally (up to 20
entries) and flush on the next successful upload.

**New structs:**
```cpp
struct __attribute__((packed)) OfflineReading {
  uint32_t timestamp;   // UTC epoch
  int16_t  tempX10;     // temp * 10
  uint8_t  humidity;    // 0-100
  uint16_t ldrRaw;      // raw ADC
  uint16_t lux;         // estimated lux
  float    accel;       // accel total
};
struct __attribute__((packed)) OfflineQueue {
  uint8_t version = 1;
  uint8_t count = 0;    // 0-20
  OfflineReading entries[20];
};
```

NVS namespace: `offline_v1`

**New functions:**
- `appendOfflineReading(float temp, float hum, int ldrRaw, int lux, float accel, uint32_t ts)` — queues entry, FIFO-overwrites oldest at cap
- `flushOfflineQueue(WiFiClientSecure&, HTTPClient&)` — posts all queued entries as a JSON array to `room_readings` endpoint; clears on HTTP 2xx

**Changes to `runMeasurementFlow()`:**
1. WiFi-fail path (currently: show error + return): instead call
   `appendOfflineReading(...)` before returning, show "QUEUED" on OLED
2. WiFi-success path: call `flushOfflineQueue()` BEFORE the current
   reading upload; if flush partially fails (e.g. some entries), keep
   un-flushed entries

**Device ID**: offline readings need `device_id` -- already defaults to
`"esp32_01"` in Supabase schema.

---

## 3. Alarm / Countdown Timer (firmware)

**What:** New TIMER menu item. User sets a preset duration (5/10/15/25/30
min) via short press cycling, starts via long press. Timer state persists
across deep sleep via `RTC_DATA_ATTR`. On expiry, LED flashes + OLED shows
a TIMER DONE screen.

**New globals (RTC_DATA_ATTR):**
- `bool timerActive = false`
- `time_t timerEndEpoch = 0` (UTC seconds)

**New functions:**
- `showTimerPage()` — page controller: displays preset selector, then
  countdown once started
- `drawTimerScreen(int secsRemaining, int totalSecs)` — shows MM:SS large,
  a fill-bar showing progress, "HOLD TO START/CANCEL"
- `drawTimerDone()` — full-screen notification, LED flash loop until
  dismissed by button press

**Menu integration:**
- `PAGE_TIMER` added after `PAGE_TREND` in `MenuPage` enum
- New icon: reuse `ICON_INTERVAL_LG` (hourglass/clock concept)
- Timer expired check in `monitorTask` startup: if `timerActive && time_now >= timerEndEpoch`, show done screen before entering normal menu loop
- Requires `locationSynced` to be true (needs UTC time); show locked screen otherwise (same pattern as Clock screen)

---

## 4. Calendar Heatmap (dashboard)

**What:** A 3-month day grid below the Sensor Details section showing daily
average comfort score (colour-coded, GitHub-contribution-style). Each cell
is a square coloured on a 5-level scale from neutral → best comfort.

**New component:** `dashboard/src/components/CalendarHeatmap.tsx`
- Props: `readings: Reading[]`
- Derives daily averages client-side from the existing readings array (the
  "month"/"year" timeframe fetches already have sufficient data; "day/week"
  will show a rolling 30-day window regardless of timeframe selector)
- Color scale: 5 levels mapped to the existing indigo palette:
  `#1f1f23` (no data) → `#312e81` → `#4338ca` → `#6366f1` → `#818cf8`
- Tooltip on hover: date + comfort score + avg temp/humidity
- Layout: 3-month grid, weeks as columns, days as rows

**Integration:** New card in `DashboardView.tsx` between the Sensor Details
stat chips and the Trend Analysis card.

---

## 5. Compare Mode (dashboard)

**What:** A "Compare" toggle button in the Trend Analysis card header.
When active, fetches the equivalent prior period and overlays it as dashed
semi-transparent lines behind the solid current-period lines.

**Changes:**
- `page.tsx`: `const [useCompare, setUseCompare] = useState(false)` +
  second Supabase query for `[prevStart, prevEnd]` inside `doFetch` when
  `useCompare` is true, stored in `prevReadings: Reading[]`
- `DashboardView.tsx`: accept optional `prevReadings` prop + toggle button
  alongside period navigation ("⧉ Compare" icon button)
- `TrendChart.tsx`: accept optional `prevReadings` prop; when present, add
  3 extra series with `lineStyle: { type: 'dashed', opacity: 0.35 }` using
  the same y-axes, time-shifted so the prior period aligns visually with
  the current period (shift by `currentStart - prevStart` ms)

---

## Execution order

Fix A, Fix B → immediate (pre-feature cleanup)
1. PWA → XS, dashboard-only
2. Offline queue → S, firmware-only
3. Alarm timer → M, firmware-only
4. Calendar heatmap → M, dashboard-only
5. Compare mode → M, dashboard-only


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
