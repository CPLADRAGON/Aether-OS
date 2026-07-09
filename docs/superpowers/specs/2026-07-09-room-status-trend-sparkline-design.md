# Room Status + On-Device Trend Sparkline — Design Spec

**Date:** 2026-07-09
**Scope:** Firmware only (`firmware/src/main.cpp`, `firmware/src/display_manager.{h,cpp}`)
**Status:** Approved for planning

## Problem

The device currently has no way to see a snapshot of current conditions or a short-term trend without opening the dashboard or Telegram bot — both require the reading to have already been uploaded, and both require the viewer to be away from the physical device. There's also no local memory of recent readings; each measurement is upload-and-forget.

## Goals

1. **ROOM** — a menu page giving an instant, offline, "what's the room like right now" glance: temperature, humidity, a plain-language comfort tag, and ambient light level.
2. **TREND** — a menu page showing a short local history (sparkline) of temperature, humidity, and light across the last 12 completed measurements, so trend direction is visible without the dashboard.

## Non-goals

- No cloud round-trip for either page (both are local-only, by design, for speed and battery).
- No float-based heat-index calculation (that's the dashboard's job) — ROOM uses simple threshold tags instead.
- No changes to the existing MEASURE flow's upload behavior, timing, or Supabase schema.
- No new hardware.

## Approach

**Local-only NVS ring buffer**, populated exclusively by completed MEASURE flows (manual or auto-wake trigger), read back instantly with no network dependency.

Rejected alternatives:
- *Pull history from Supabase on-demand* — requires WiFi + round trip every TREND open; adds latency and battery cost; duplicates what the dashboard already does well.
- *Hybrid local+cloud* — unnecessary complexity for a 64×48 sparkline.

## Design

### 1. Data model — new `trend_v1` NVS namespace

```cpp
struct __attribute__((packed)) TrendPoint {
  int16_t  tempX10;   // temp * 10, e.g. 235 = 23.5C
  uint8_t  humidity;   // 0-100
  uint16_t ldr;        // raw analogRead() 0-4095
};

struct __attribute__((packed)) TrendHistory {
  uint8_t version = 1;
  uint8_t count;         // valid entries, 0-12
  uint8_t head;          // next write index (ring buffer, wraps at 12)
  TrendPoint points[12]; // ~62 bytes total NVS footprint
};
```

- Loaded once at boot (`monitorTask` init, alongside existing config/stats loads).
- Kept resident in RAM for the awake session; appended and immediately persisted to NVS after every successful `runMeasurementFlow()` average (`validCount > 0`), **regardless of Supabase upload success/failure** — this is local data, independent of connectivity.
- A single-sample ROOM read does **not** append to this buffer (keeps history limited to the quality-checked 5-sample MEASURE average, not noisy one-shot reads).
- If NVS write fails, log via `Serial.printf` and continue — trend data loss is acceptable; the core measure/upload path must never be blocked by this.

### 2. ROOM page (Room Status Digest)

- Triggered from the menu like any other page (`PAGE_ROOM`).
- On open: single DHT11 read (temperature + humidity) + single `analogRead(LDR_PIN)`. No MPU6050, no WiFi, no upload. Target latency ~250ms (DHT11 read time).
- Layout reuses the existing `drawColumnValue()` helper (already shared by Weather/Stats/Measure): **TEMP | HUM** two-column with big numeric values, units in the label row.
- Footer (inverted bar, matching existing screen conventions): a combined comfort tag from independent temp/humidity buckets — no float math:
  - Temp: `<18 COLD` · `18–23 COOL` · `23–28 WARM` · `>28 HOT`
  - Humidity: `<40 DRY` · `40–60 NORMAL` · `>60 HUMID`
  - Combined as `"<TEMP TAG>+<HUMIDITY TAG>"`, e.g. `WARM+HUMID`, `COOL+DRY`.
- Small light-level tag (`DARK` / `DIM` / `BRIGHT`) shown in place of the WiFi icon slot in the header (ROOM doesn't need WiFi status since it makes no network calls). LDR thresholds are tunable constants (`ROOM_LDR_DARK_MAX`, `ROOM_LDR_BRIGHT_MIN`) — initial values are placeholders to be tuned against the actual sensor on hardware during verification; this is an explicit, named TBD, not a design gap.
- Sensor read failure (NaN from DHT11): reuse existing `drawErrorScreen("ROOM", "SENSOR", "READ FAIL")` pattern, matching how other pages already handle DHT failures.

### 3. TREND page (On-Device Sparkline)

- Triggered from the menu (`PAGE_TREND`).
- Single page with three sub-views cycled by **short button press**: `TEMP → HUM → LIGHT → (loop)`. **Long press** exits back to the menu — same interaction pattern as `showSavedWiFi()` and other existing subpages.
- Per sub-view:
  - Header: metric name + delta arrow vs. the immediately previous point (`▲` rising / `▼` falling / `–` flat, with a small tolerance band to avoid arrow flicker on near-equal values).
  - Body: a polyline sparkline across up to 12 points, auto-scaled to that sub-view's own min/max in the current buffer (each metric scaled independently, not shared axes).
  - Footer: latest numeric value with its unit.
- **Edge case — insufficient data:** if `TrendHistory.count < 2`, show a placeholder screen ("TREND", "NOT ENOUGH", "DATA YET") instead of attempting to draw a degenerate/empty graph. This is the expected state on a freshly flashed device until at least 2 measurements complete.
- Rendering reuses the existing `dm::` primitives (`drawLine`, `drawText`, `drawHeader`) — no new display_manager primitives required beyond what's already in place from the motion pass.

### 4. Menu integration

- Two new `MenuPage` enum values: `PAGE_ROOM`, `PAGE_TREND`.
- `TOTAL_MENU_ITEMS` increases from 10 to 12.
- Two new 24×24 cover-flow XBM icons:
  - ROOM: simple house/room glyph.
  - TREND: rising bars or line-chart glyph (visually distinct from the existing `ICON_STATS_LG` bar-chart icon used for the STATS page).
- Both wired into the existing `triggerAction` dispatch inside `monitorTask`'s button-handling loop, following the same `g_menuOwnedByPage = true/false` bracket pattern established for other subpages (this was the fix for the menu/subpage render-race bugs found earlier in the session) so `uiTask` doesn't attempt to render the cover-flow menu underneath these pages.

### 5. Error handling / edge cases summary

| Case | Handling |
|---|---|
| DHT11 read failure on ROOM | `drawErrorScreen("ROOM", "SENSOR", "READ FAIL")`, wait, return to menu |
| TREND with 0–1 stored points | Placeholder screen, no graph draw attempted |
| NVS write failure appending a point | Logged via `Serial.printf`, MEASURE flow continues unaffected |
| NVS read failure/corruption at boot | Treat as empty history (`count = 0`), do not crash |

### 6. Testing

No unit test infrastructure exists in this firmware (Arduino/PlatformIO, no test runner configured). Verification is manual, on hardware:

- Flash, take several manual MEASURE runs to populate history, confirm TREND sparkline appears and scales sensibly across all three sub-views.
- Confirm ROOM reads complete quickly and never attempt a WiFi connection.
- Confirm cold-boot (empty history) shows the "NOT ENOUGH DATA YET" placeholder rather than a broken graph.
- Confirm long-idle-then-sleep cycles don't corrupt the NVS ring buffer (spot-check via Serial logging of `TrendHistory.count`/`head` across several sleep/wake cycles).
- Confirm menu navigation (12 items now) still auto-sleeps correctly and cover-flow icons for ROOM/TREND render without clipping, consistent with prior icon-alignment fixes this session.

## Deliverables

- `firmware/src/main.cpp`: `TrendHistory` struct + NVS load/save, `PAGE_ROOM`/`PAGE_TREND` enum + dispatch, `drawRoomStatus()`, `drawTrendSparkline()` renderers, LDR/temp/humidity threshold constants.
- `firmware/src/display_manager.h/.cpp`: two new 24×24 XBM icons (`ICON_ROOM_LG`, `ICON_TREND_LG`).
- No dashboard, Telegram bot, or Supabase schema changes.
