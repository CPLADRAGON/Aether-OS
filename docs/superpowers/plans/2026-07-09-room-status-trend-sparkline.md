# Room Status + On-Device Trend Sparkline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two new firmware menu pages — ROOM (instant local room-condition snapshot) and TREND (12-point on-device sparkline of temp/humidity/light) — backed by a new NVS-persisted ring buffer that survives deep sleep.

**Architecture:** A packed `TrendHistory` struct (12-entry ring buffer of temp/humidity/light) is persisted to a new `trend_v1` NVS namespace, following the exact pattern already used by `LogQueue`/`logs_v2`. It's appended once per successful `runMeasurementFlow()` average, independent of WiFi/upload outcome. ROOM takes a fresh instant DHT11+LDR reading with no NVS/WiFi involvement. Both pages reuse existing `dm::` primitives and the `drawColumnValue()` helper already shared by Weather/Stats/Measure screens.

**Tech Stack:** ESP32 / Arduino / PlatformIO, U8g2 (via the `dm::` display_manager abstraction), ESP32 `Preferences` (NVS).

**Spec:** `docs/superpowers/specs/2026-07-09-room-status-trend-sparkline-design.md`

**Note on testing:** This firmware has no unit test runner (Arduino/PlatformIO, confirmed in the spec). Every task's "test" step is a `pio run` build-verification instead of a unit test — this is the correct equivalent for this codebase, not a shortcut. Final manual on-hardware verification is Task 8.

---

### Task 1: Add ROOM/TREND icon enum values

**Files:**
- Modify: `firmware/src/display_manager.h:19-40`

- [ ] **Step 1: Add two new Icon enum values**

In `firmware/src/display_manager.h`, find this exact block:

```cpp
enum Icon : uint8_t {
    ICON_WIFI = 0,
    ICON_CLOUD,
    ICON_PIN,
    ICON_SCAN,
    // 24x24 page icons for cover-flow menu (Phase 4)
    ICON_MEASURE_LG,
    ICON_TIME_LG,
    ICON_WEATHER_LG,
    ICON_LOCATE_LG,
    ICON_LED_LG,
    ICON_INTERVAL_LG,
    ICON_STATS_LG,
    ICON_WIFIMENU_LG,
    ICON_RESET_LG,
    ICON_SLEEP_LG,
    ICON_PORTAL_LG,
    ICON_CLEAR_LG,
    ICON_SELECT_LG,
    ICON_BACK_LG,
    ICON_COUNT
};
```

Replace it with (adds `ICON_ROOM_LG` and `ICON_TREND_LG` right after `ICON_STATS_LG`, keeping every other value unchanged):

```cpp
enum Icon : uint8_t {
    ICON_WIFI = 0,
    ICON_CLOUD,
    ICON_PIN,
    ICON_SCAN,
    // 24x24 page icons for cover-flow menu (Phase 4)
    ICON_MEASURE_LG,
    ICON_TIME_LG,
    ICON_WEATHER_LG,
    ICON_LOCATE_LG,
    ICON_LED_LG,
    ICON_INTERVAL_LG,
    ICON_STATS_LG,
    ICON_ROOM_LG,
    ICON_TREND_LG,
    ICON_WIFIMENU_LG,
    ICON_RESET_LG,
    ICON_SLEEP_LG,
    ICON_PORTAL_LG,
    ICON_CLEAR_LG,
    ICON_SELECT_LG,
    ICON_BACK_LG,
    ICON_COUNT
};
```

- [ ] **Step 2: Commit**

```bash
git add firmware/src/display_manager.h
git commit -m "feat(firmware): add ICON_ROOM_LG/ICON_TREND_LG enum values"
```

---

### Task 2: Add ROOM/TREND XBM icon bitmaps

**Files:**
- Modify: `firmware/src/display_manager.cpp` (icon bitmap block + `icon24_xbm()` switch)

- [ ] **Step 1: Add the two new 24x24 XBM bitmaps**

In `firmware/src/display_manager.cpp`, find this exact block (the last icon bitmap before `icon24_xbm()`):

```cpp
// back (arrow left)
static const uint8_t xbm_back_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x80,0x00, 0x00,0xC0,0x00,
    0x00,0xE0,0x00, 0x00,0xF0,0x00, 0x00,0xF8,0x00, 0xFC,0xFF,0x0F,
    0xFE,0xFF,0x0F, 0xFF,0xFF,0x0F, 0xFF,0xFF,0x0F, 0xFE,0xFF,0x0F,
    0xFC,0xFF,0x0F, 0x00,0xF8,0x00, 0x00,0xF0,0x00, 0x00,0xE0,0x00,
    0x00,0xC0,0x00, 0x00,0x80,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};

static const uint8_t *icon24_xbm(dm::Icon i) {
```

Replace it with (adds `xbm_room_lg` and `xbm_trend_lg` — house glyph with peaked roof + door, and a rising line-chart glyph with axis — both generated programmatically and verified bit-by-bit, not hand-encoded):

```cpp
// back (arrow left)
static const uint8_t xbm_back_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x80,0x00, 0x00,0xC0,0x00,
    0x00,0xE0,0x00, 0x00,0xF0,0x00, 0x00,0xF8,0x00, 0xFC,0xFF,0x0F,
    0xFE,0xFF,0x0F, 0xFF,0xFF,0x0F, 0xFF,0xFF,0x0F, 0xFE,0xFF,0x0F,
    0xFC,0xFF,0x0F, 0x00,0xF8,0x00, 0x00,0xF0,0x00, 0x00,0xE0,0x00,
    0x00,0xC0,0x00, 0x00,0x80,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};

// room: house with peaked roof + outline walls + door
static const uint8_t xbm_room_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x18,0x00, 0x00,0x3C,0x00,
    0x00,0x7E,0x00, 0x00,0xFF,0x00, 0x80,0xFF,0x01, 0xC0,0xFF,0x03,
    0xE0,0xFF,0x07, 0xF0,0xFF,0x0F, 0x10,0x00,0x08, 0x10,0x00,0x08,
    0x10,0x00,0x08, 0x10,0x3C,0x08, 0x10,0x3C,0x08, 0x10,0x3C,0x08,
    0x10,0x3C,0x08, 0x10,0x3C,0x08, 0x10,0x3C,0x08, 0x10,0x3C,0x08,
    0xF0,0xFF,0x0F, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};
// trend: rising line-chart with axis
static const uint8_t xbm_trend_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x08,0x00,0x08, 0x08,0x00,0x04, 0x08,0x00,0x02, 0x08,0x00,0x01,
    0x08,0x80,0x00, 0x08,0x40,0x00, 0x08,0x20,0x00, 0x08,0x10,0x00,
    0x08,0x08,0x00, 0x08,0x04,0x00, 0x08,0x02,0x00, 0x08,0x01,0x00,
    0xC8,0x00,0x00, 0x28,0x00,0x00, 0x18,0x00,0x00, 0x08,0x00,0x00,
    0xF8,0xFF,0x3F, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};

static const uint8_t *icon24_xbm(dm::Icon i) {
```

- [ ] **Step 2: Wire the two new icons into the switch statement**

Find this exact block:

```cpp
        case dm::ICON_SELECT_LG:   return xbm_select_lg;
        case dm::ICON_BACK_LG:     return xbm_back_lg;
        default:                   return xbm_measure_lg;
    }
}
```

Replace it with:

```cpp
        case dm::ICON_SELECT_LG:   return xbm_select_lg;
        case dm::ICON_BACK_LG:     return xbm_back_lg;
        case dm::ICON_ROOM_LG:     return xbm_room_lg;
        case dm::ICON_TREND_LG:    return xbm_trend_lg;
        default:                   return xbm_measure_lg;
    }
}
```

- [ ] **Step 3: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS` at the end, no errors mentioning `xbm_room_lg`, `xbm_trend_lg`, `ICON_ROOM_LG`, or `ICON_TREND_LG`.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/display_manager.cpp
git commit -m "feat(firmware): add ROOM/TREND 24x24 icon bitmaps"
```

---

### Task 3: Add TrendHistory data model + NVS load/append functions

**Files:**
- Modify: `firmware/src/main.cpp` (struct block near `LogQueue`, function block near `clearLogQueue()`)

- [ ] **Step 1: Add TrendPoint/TrendHistory structs and global instance**

Find this exact block:

```cpp
struct __attribute__((packed)) LogQueue {
  uint8_t version = 1;
  uint8_t count = 0;
  SessionLog logs[10];
};

WiFiSnapshot currentSnapshot;
DeviceConfig config;
LogQueue logQueue;
uint32_t sessionStartTime = 0; // Relative (millis) or Absolute (UTC)
bool timeSynced = false;
```

Replace it with:

```cpp
struct __attribute__((packed)) LogQueue {
  uint8_t version = 1;
  uint8_t count = 0;
  SessionLog logs[10];
};

// --- Local Trend History (ROOM/TREND menu features) ---
// Ring buffer of the last 12 completed MEASURE averages. `head` is the index
// of the most-recently-written point (not "next write index"); this keeps
// the read-side chronological-order math simple and unambiguous.
struct __attribute__((packed)) TrendPoint {
  int16_t  tempX10; // temperature * 10, e.g. 235 = 23.5C
  uint8_t  humidity; // 0-100
  uint16_t ldr;      // raw analogRead(LDR_PIN), 0-4095
};

struct __attribute__((packed)) TrendHistory {
  uint8_t version = 1;
  uint8_t count = 0; // valid entries, 0-12
  uint8_t head = 0;  // index of the most recently written point
  TrendPoint points[12];
};

WiFiSnapshot currentSnapshot;
DeviceConfig config;
LogQueue logQueue;
TrendHistory trendHistory;
uint32_t sessionStartTime = 0; // Relative (millis) or Absolute (UTC)
bool timeSynced = false;
```

- [ ] **Step 2: Add loadTrendHistory() and appendTrendPoint() functions**

Find this exact block:

```cpp
void clearLogQueue() {
  preferences.begin("logs_v2", false);
  logQueue.count = 0;
  preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
  preferences.end();
  Serial.println("[NVS] Log queue cleared.");
}

void migrateNVS() {
```

Replace it with:

```cpp
void clearLogQueue() {
  preferences.begin("logs_v2", false);
  logQueue.count = 0;
  preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
  preferences.end();
  Serial.println("[NVS] Log queue cleared.");
}

// Loads trendHistory from NVS at boot. If the stored blob is missing, the
// wrong size, or a different version, resets to an empty (count=0) history
// rather than risk reading garbage data into the ring buffer.
void loadTrendHistory() {
  preferences.begin("trend_v1", true);
  size_t len = preferences.getBytes("hist", &trendHistory, sizeof(TrendHistory));
  preferences.end();
  if (len != sizeof(TrendHistory) || trendHistory.version != 1) {
    trendHistory.version = 1;
    trendHistory.count = 0;
    trendHistory.head = 0;
  }
}

// Appends one averaged MEASURE reading to the local trend ring buffer and
// persists it immediately. Called regardless of WiFi/upload outcome — this
// is local-only data, independent of connectivity. Non-fatal on NVS failure:
// the core measure/upload path must never be blocked by this.
void appendTrendPoint(float tempC, float humidityPct, int ldrRaw) {
  int writeIdx = (trendHistory.count == 0) ? 0 : (trendHistory.head + 1) % 12;

  int tempTenths = (int)(tempC * 10.0f + (tempC >= 0 ? 0.5f : -0.5f));
  if (tempTenths > 32767) tempTenths = 32767;
  if (tempTenths < -32768) tempTenths = -32768;

  int humInt = (int)(humidityPct + 0.5f);
  if (humInt < 0) humInt = 0;
  if (humInt > 100) humInt = 100;

  trendHistory.points[writeIdx].tempX10 = (int16_t)tempTenths;
  trendHistory.points[writeIdx].humidity = (uint8_t)humInt;
  trendHistory.points[writeIdx].ldr = (uint16_t)ldrRaw;
  trendHistory.head = (uint8_t)writeIdx;
  if (trendHistory.count < 12) trendHistory.count++;

  preferences.begin("trend_v1", false);
  preferences.putBytes("hist", &trendHistory, sizeof(TrendHistory));
  preferences.end();
  Serial.printf("[TREND] Point appended. count=%d head=%d\n",
                trendHistory.count, trendHistory.head);
}

void migrateNVS() {
```

- [ ] **Step 3: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`. (These functions aren't called yet, so an "unused function" warning is fine — it will disappear once Task 4/6/7 wire them in.)

- [ ] **Step 4: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): add TrendHistory NVS ring buffer (trend_v1)"
```

---

### Task 4: Add PAGE_ROOM/PAGE_TREND to the menu enum and arrays

**Files:**
- Modify: `firmware/src/main.cpp` (`MenuPage` enum, `menuItems[]`, `menuIcons[]`, `TOTAL_MENU_ITEMS`)

- [ ] **Step 1: Add the two new MenuPage enum values and bump TOTAL_MENU_ITEMS**

Find this exact block:

```cpp
enum MenuPage {
  PAGE_MEASURE,
  PAGE_TIME,
  PAGE_WEATHER,
  PAGE_LOCATE,
  PAGE_LED,
  PAGE_INTERVAL,
  PAGE_STATS,
  PAGE_PORTAL,
  PAGE_RESET,
  PAGE_SLEEP
};
const int TOTAL_MENU_ITEMS = 10;
const char *menuItems[] = {"MEASURE",     "TIME",      "WEATHER", "LOCATE",
                           "LED",         "INTERVAL",  "STATS",   "WIFI MENU",
                           "RESET STATS", "DEEP SLEEP"};
```

Replace it with (inserts `PAGE_ROOM`/`PAGE_TREND` right after `PAGE_STATS`, so enum order, `menuItems[]` order, and `menuIcons[]` order in Task 4 Step 2 all stay index-parallel):

```cpp
enum MenuPage {
  PAGE_MEASURE,
  PAGE_TIME,
  PAGE_WEATHER,
  PAGE_LOCATE,
  PAGE_LED,
  PAGE_INTERVAL,
  PAGE_STATS,
  PAGE_ROOM,
  PAGE_TREND,
  PAGE_PORTAL,
  PAGE_RESET,
  PAGE_SLEEP
};
const int TOTAL_MENU_ITEMS = 12;
const char *menuItems[] = {"MEASURE",     "TIME",       "WEATHER",     "LOCATE",
                           "LED",         "INTERVAL",   "STATS",       "ROOM",
                           "TREND",       "WIFI MENU",  "RESET STATS", "DEEP SLEEP"};
```

- [ ] **Step 2: Add the two new icons to menuIcons[], keeping index-parallel with menuItems[]**

Find this exact block:

```cpp
static const dm::Icon menuIcons[] = {
    dm::ICON_MEASURE_LG, dm::ICON_TIME_LG, dm::ICON_WEATHER_LG, dm::ICON_LOCATE_LG,
    dm::ICON_LED_LG, dm::ICON_INTERVAL_LG, dm::ICON_STATS_LG, dm::ICON_WIFIMENU_LG,
    dm::ICON_RESET_LG, dm::ICON_SLEEP_LG
};
```

Replace it with:

```cpp
static const dm::Icon menuIcons[] = {
    dm::ICON_MEASURE_LG, dm::ICON_TIME_LG, dm::ICON_WEATHER_LG, dm::ICON_LOCATE_LG,
    dm::ICON_LED_LG, dm::ICON_INTERVAL_LG, dm::ICON_STATS_LG, dm::ICON_ROOM_LG,
    dm::ICON_TREND_LG, dm::ICON_WIFIMENU_LG, dm::ICON_RESET_LG, dm::ICON_SLEEP_LG
};
```

- [ ] **Step 3: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`. At this point the menu will show 12 items (ROOM/TREND appear in the cover-flow with their new icons) but selecting them does nothing yet — that's expected and safe; dispatch is wired in Task 5/6.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): add ROOM/TREND to menu enum and cover-flow arrays"
```

---

### Task 5: Implement ROOM page (Room Status Digest)

**Files:**
- Modify: `firmware/src/main.cpp` (threshold constants, `drawRoomStatus()`/`showRoomPage()` functions, dispatch wiring)

- [ ] **Step 1: Add ROOM comfort/light threshold constants**

Find this exact block:

```cpp
#define DHTPIN 4
#define DHTTYPE DHT11
#define BUTTON_PIN 33
#define LDR_PIN 34
#define RED_PIN 13
#define GREEN_PIN 14
#define BLUE_PIN 27
#define OLED_RST 16
```

Replace it with:

```cpp
#define DHTPIN 4
#define DHTTYPE DHT11
#define BUTTON_PIN 33
#define LDR_PIN 34
#define RED_PIN 13
#define GREEN_PIN 14
#define BLUE_PIN 27
#define OLED_RST 16

// ROOM page comfort/light thresholds. These are starting values, not
// calibrated to any specific sensor unit — tune against your actual DHT11
// and LDR during the Task 8 hardware verification pass.
#define ROOM_TEMP_COLD_MAX  18   // <18C  = COLD
#define ROOM_TEMP_COOL_MAX  23   // <23C  = COOL (else WARM below HOT max)
#define ROOM_TEMP_WARM_MAX  28   // <28C  = WARM, >=28C = HOT
#define ROOM_HUM_DRY_MAX    40   // <40%  = DRY
#define ROOM_HUM_NORMAL_MAX 60   // <60%  = NORMAL, >=60% = HUMID
#define ROOM_LDR_DARK_MAX   500  // <500  = DARK
#define ROOM_LDR_BRIGHT_MIN 2000 // >=2000 = BRIGHT, between = DIM
```

- [ ] **Step 2: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`.

- [ ] **Step 3: Add drawRoomStatus() and showRoomPage()**

Find this exact block (end of `showStatsPage()`):

```cpp
void showStatsPage() {
  uint32_t totalMin = config.totalLifetimeRuntime / 60;
  drawStatsScreen(measureCount, bootCount, totalMin);
  waitWithButtonPoll(5000);
}
```

Replace it with:

```cpp
void showStatsPage() {
  uint32_t totalMin = config.totalLifetimeRuntime / 60;
  drawStatsScreen(measureCount, bootCount, totalMin);
  waitWithButtonPoll(5000);
}

// ROOM: instant local DHT11+LDR snapshot, no WiFi/upload, no history append
// (kept separate from the quality-checked 5-sample MEASURE average).
static void drawRoomStatus(float tempC, int humPct, int ldrRaw,
                           const char *comfortTag, const char *lightTag) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "ROOM", false, dm::ICON_WIFI);

  // Light-level tag in place of the WiFi icon slot (ROOM makes no network
  // calls, so the WiFi indicator would be misleading here).
  dm::setFont(dm::FONT_SMALL);
  int ltw = dm::textWidth(lightTag);
  dm::drawTextInverted(OLED_OFFSET_X + OLED_W - ltw - 2, OLED_OFFSET_Y + 2, lightTag);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%d", tInt);
  snprintf(humBuf, sizeof(humBuf), "%d", humPct);

  drawColumnValue(OLED_OFFSET_X + 2, 28, OLED_OFFSET_Y + 11, "TEMP C", tempBuf);
  dm::drawVLine(OLED_OFFSET_X + 32, OLED_OFFSET_Y + 11, 26);
  drawColumnValue(OLED_OFFSET_X + 34, 28, OLED_OFFSET_Y + 11, "HUM %", humBuf);

  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  int cw = dm::textWidth(comfortTag);
  int cx = OLED_OFFSET_X + (OLED_W - cw) / 2;
  if (cx < 2) cx = 2;
  dm::drawTextInverted(cx, OLED_OFFSET_Y + OLED_H - 8, comfortTag);
  dm::endFrame();
}

void showRoomPage() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int ldrRaw = analogRead(LDR_PIN);

  if (isnan(t) || isnan(h)) {
    drawErrorScreen("ROOM", "SENSOR", "READ FAIL");
    waitWithButtonPoll(2000);
    return;
  }

  int tInt = (int)(t + 0.5f);
  const char *tempTag = (tInt < ROOM_TEMP_COLD_MAX)   ? "COLD"
                       : (tInt < ROOM_TEMP_COOL_MAX)   ? "COOL"
                       : (tInt < ROOM_TEMP_WARM_MAX)   ? "WARM"
                                                        : "HOT";
  int hInt = (int)(h + 0.5f);
  const char *humTag = (hInt < ROOM_HUM_DRY_MAX)      ? "DRY"
                      : (hInt < ROOM_HUM_NORMAL_MAX)   ? "NORMAL"
                                                        : "HUMID";
  char comfortTag[16];
  snprintf(comfortTag, sizeof(comfortTag), "%s+%s", tempTag, humTag);

  const char *lightTag = (ldrRaw < ROOM_LDR_DARK_MAX)     ? "DARK"
                        : (ldrRaw < ROOM_LDR_BRIGHT_MIN)   ? "DIM"
                                                             : "BRIGHT";

  drawRoomStatus(t, hInt, ldrRaw, comfortTag, lightTag);
  waitWithButtonPoll(5000);
}
```

- [ ] **Step 4: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`. (`showRoomPage()` isn't called yet — Step 5 wires the dispatch.)

- [ ] **Step 5: Wire PAGE_ROOM into the menu dispatch**

Find this exact block:

```cpp
          } else if (currentMenuIndex == PAGE_STATS)
            showStatsPage();
          else if (currentMenuIndex == PAGE_PORTAL) {
```

Replace it with:

```cpp
          } else if (currentMenuIndex == PAGE_STATS)
            showStatsPage();
          else if (currentMenuIndex == PAGE_ROOM)
            showRoomPage();
          else if (currentMenuIndex == PAGE_PORTAL) {
```

- [ ] **Step 6: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): implement ROOM page (instant status digest)"
```

---

### Task 6: Implement TREND page (On-Device Sparkline)

**Files:**
- Modify: `firmware/src/main.cpp` (`drawTrendSparkline()`/`showTrendPage()` functions, dispatch wiring)

- [ ] **Step 1: Add drawTrendSparkline() and showTrendPage()**

Find this exact block (end of the code added in Task 5 Step 3 — the end of `showRoomPage()`):

```cpp
  drawRoomStatus(t, hInt, ldrRaw, comfortTag, lightTag);
  waitWithButtonPoll(5000);
}
```

Replace it with:

```cpp
  drawRoomStatus(t, hInt, ldrRaw, comfortTag, lightTag);
  waitWithButtonPoll(5000);
}

enum TrendView { TREND_TEMP, TREND_HUM, TREND_LIGHT };

// Renders one sub-view of the TREND sparkline. `view` selects which metric
// from trendHistory to plot. Points are read out in chronological order
// (oldest first) using the ring-buffer math matching appendTrendPoint()'s
// "head = index of most recently written point" convention.
static void drawTrendSparkline(TrendView view) {
  if (!dm::beginFrame(portMAX_DELAY)) return;

  const char *name = (view == TREND_TEMP) ? "TEMP"
                    : (view == TREND_HUM) ? "HUM"
                                           : "LIGHT";

  if (trendHistory.count < 2) {
    dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, name, false, dm::ICON_WIFI);
    dm::setFont(dm::FONT_NORMAL);
    dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 18, "NOT ENOUGH");
    dm::setFont(dm::FONT_SMALL);
    dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 30, "DATA YET");
    dm::endFrame();
    return;
  }

  int n = trendHistory.count;
  float vals[12];
  for (int i = 0; i < n; i++) {
    int idx = (n < 12) ? i : (trendHistory.head + 1 + i) % 12;
    TrendPoint &p = trendHistory.points[idx];
    if (view == TREND_TEMP)      vals[i] = p.tempX10 / 10.0f;
    else if (view == TREND_HUM)  vals[i] = (float)p.humidity;
    else                          vals[i] = (float)p.ldr;
  }

  float delta = vals[n - 1] - vals[n - 2];
  const char *arrow = (delta > 0.3f) ? "^" : (delta < -0.3f) ? "v" : "-";
  char headBuf[16];
  snprintf(headBuf, sizeof(headBuf), "%s %s", name, arrow);
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, headBuf, false, dm::ICON_WIFI);

  float vMin = vals[0], vMax = vals[0];
  for (int i = 1; i < n; i++) {
    if (vals[i] < vMin) vMin = vals[i];
    if (vals[i] > vMax) vMax = vals[i];
  }
  float range = vMax - vMin;
  if (range < 0.01f) range = 1.0f; // flat-line guard, avoids div-by-zero

  int plotX0 = OLED_OFFSET_X + 2;
  int plotX1 = OLED_OFFSET_X + OLED_W - 2;
  int plotY0 = OLED_OFFSET_Y + 12;
  int plotY1 = OLED_OFFSET_Y + 36;
  int plotH = plotY1 - plotY0;
  int plotW = plotX1 - plotX0;

  int prevX = plotX0;
  int prevY = plotY1 - (int)(((vals[0] - vMin) / range) * plotH);
  for (int i = 1; i < n; i++) {
    int x = plotX0 + (plotW * i) / (n - 1);
    int y = plotY1 - (int)(((vals[i] - vMin) / range) * plotH);
    dm::drawLine(prevX, prevY, x, y);
    prevX = x;
    prevY = y;
  }

  char footBuf[16];
  if (view == TREND_TEMP)      snprintf(footBuf, sizeof(footBuf), "%.1fC", vals[n - 1]);
  else if (view == TREND_HUM)  snprintf(footBuf, sizeof(footBuf), "%d%%", (int)vals[n - 1]);
  else                          snprintf(footBuf, sizeof(footBuf), "%d", (int)vals[n - 1]);

  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  int fw = dm::textWidth(footBuf);
  int fx = OLED_OFFSET_X + (OLED_W - fw) / 2;
  if (fx < 2) fx = 2;
  dm::drawTextInverted(fx, OLED_OFFSET_Y + OLED_H - 8, footBuf);
  dm::endFrame();
}

void showTrendPage() {
  int view = 0; // 0=TEMP, 1=HUM, 2=LIGHT
  lastInteractionTime = millis();
  buttonEvent = false;

  while (millis() - lastInteractionTime < 20000) {
    drawTrendSparkline((TrendView)view);

    if (buttonEvent) {
      buttonEvent = false;
      view = (view + 1) % 3;
      lastInteractionTime = millis();
    }
    if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
      if (!longPressTriggered) {
        longPressTriggered = true;
        break;
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
```

- [ ] **Step 2: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`.

- [ ] **Step 3: Wire PAGE_TREND into the menu dispatch**

Find this exact block (the code added in Task 5 Step 5):

```cpp
          } else if (currentMenuIndex == PAGE_STATS)
            showStatsPage();
          else if (currentMenuIndex == PAGE_ROOM)
            showRoomPage();
          else if (currentMenuIndex == PAGE_PORTAL) {
```

Replace it with:

```cpp
          } else if (currentMenuIndex == PAGE_STATS)
            showStatsPage();
          else if (currentMenuIndex == PAGE_ROOM)
            showRoomPage();
          else if (currentMenuIndex == PAGE_TREND)
            showTrendPage();
          else if (currentMenuIndex == PAGE_PORTAL) {
```

- [ ] **Step 4: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): implement TREND page (on-device sparkline)"
```

---

### Task 7: Wire history load-at-boot and append-after-measure

**Files:**
- Modify: `firmware/src/main.cpp` (`monitorTask()` boot sequence, `runMeasurementFlow()`)

- [ ] **Step 1: Load trendHistory at boot**

Find this exact block (inside `monitorTask()`):

```cpp
  loadConfig();
  migrateNVS();
```

Replace it with:

```cpp
  loadConfig();
  migrateNVS();
  loadTrendHistory();
```

- [ ] **Step 2: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`.

- [ ] **Step 3: Append a trend point after every successful measurement average**

Find this exact block (inside `runMeasurementFlow()`):

```cpp
  if (validCount > 0) {
    uiLine1 = "LINKING...";
    if (!ensureWiFi(true)) {
```

Replace it with:

```cpp
  if (validCount > 0) {
    // Append to the local trend history immediately — this must happen
    // before any WiFi/upload attempt so a connectivity failure never causes
    // the on-device history to silently fall behind reality.
    appendTrendPoint(totalT / validCount, totalH / validCount,
                     (int)(totalL / validCount));

    uiLine1 = "LINKING...";
    if (!ensureWiFi(true)) {
```

- [ ] **Step 4: Build to verify no compile errors**

Run: `cd firmware; pio run`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): load trend history at boot, append after each measurement"
```

---

### Task 8: Manual hardware verification

**Files:** none (verification only)

- [ ] **Step 1: Restore real credentials**

Confirm `firmware/include/secrets.h` has your real `WIFI_SSID`, `WIFI_PASS`, and `SUPABASE_KEY` (not the `REPLACE_ME` placeholders) before flashing, otherwise MEASURE's upload step will fail (this does not affect ROOM/TREND, which are local-only).

- [ ] **Step 2: Flash and monitor**

Run: `cd firmware; pio run -t upload; pio device monitor`
Expected: build succeeds, upload succeeds, serial monitor connects.

- [ ] **Step 3: Verify cold-boot TREND placeholder**

On a freshly-flashed device (or after clearing NVS), navigate to TREND from the main menu.
Expected: shows "NOT ENOUGH / DATA YET" on all three sub-views (short-press cycles TEMP→HUM→LIGHT), no garbled graph, no crash.

- [ ] **Step 4: Verify ROOM is instant and offline**

Navigate to ROOM from the main menu.
Expected: page renders within ~1 second, no WiFi connection attempt is visible in the serial log (no `[WIFI]` lines), shows TEMP | HUM columns plus a comfort tag footer (e.g. `WARM+HUMID`) and a light tag in the header-right slot (`DARK`/`DIM`/`BRIGHT`).

- [ ] **Step 5: Populate trend history and verify the sparkline**

Run MEASURE (manual trigger) at least 3 times, waiting for each to complete (including the sync/upload step — completes even if it ultimately fails, since the trend point is appended before the WiFi attempt).

After 3+ measurements, open TREND again.
Expected: all three sub-views (TEMP/HUM/LIGHT) now show a connected line graph instead of the placeholder, the header shows a `^`/`v`/`-` arrow reflecting the most recent change, and the footer shows the latest numeric value with correct units (`C`, `%`, raw number for light).

- [ ] **Step 6: Verify ring buffer survives deep sleep**

Note the serial log line `[TREND] Point appended. count=N head=N` after a MEASURE. Trigger deep sleep (DEEP SLEEP menu item), then wake the device again (button press) and immediately open TREND.
Expected: the previously-recorded points are still present (graph is not reset to "NOT ENOUGH DATA YET"), confirming the NVS round-trip survived the sleep/wake cycle.

- [ ] **Step 7: Verify 12-point wraparound**

Continue running MEASURE until more than 12 measurements have completed (check the `count=` value in serial logs — it should cap at 12 and stop incrementing, while `head` keeps advancing and wrapping 0-11).
Expected: TREND continues to show a smooth 12-point graph (oldest point silently drops off as new ones are added), no visual corruption or duplicate points.

- [ ] **Step 8: Tune LDR/temperature thresholds if needed**

Compare the `DARK`/`DIM`/`BRIGHT` tag and `COLD`/`COOL`/`WARM`/`HOT` + `DRY`/`NORMAL`/`HUMID` tags against your actual room conditions. If they feel wrong (e.g. your room reads `BRIGHT` in normal indoor lighting), adjust the `ROOM_LDR_DARK_MAX`/`ROOM_LDR_BRIGHT_MIN`/`ROOM_TEMP_*`/`ROOM_HUM_*` constants added in Task 5 Step 1, rebuild, and reflash.

- [ ] **Step 9: Commit any threshold tuning**

```bash
git add firmware/src/main.cpp
git commit -m "chore(firmware): tune ROOM thresholds against real hardware"
```

(Skip this commit if no threshold changes were needed.)
