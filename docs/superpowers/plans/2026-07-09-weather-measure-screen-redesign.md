# Weather & Measure Screen Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the Weather and Measure-sample OLED screens their own visual identity (icon-left, stats-stacked-right layout) instead of the generic two-column layout they currently share with the unrelated Stats screen.

**Architecture:** Add 4 new weather-condition icon slots (sun/rain/storm/snow) alongside the existing cloud icon, add a small mapping function from OpenWeatherMap's condition string to the right icon, then rewrite the two screens' drawing functions to the new layout. The 4 new icon bitmaps are placeholder (all-zero) until the user supplies real converted artwork — the plan documents exactly what to swap in and where.

**Tech Stack:** PlatformIO / Arduino, ESP32, U8g2 (64x48 monochrome OLED).

**Spec:** `docs/superpowers/specs/2026-07-09-weather-measure-screen-redesign-design.md`

**Verification:** No test runner exists for this firmware project — every task's verification step is `pio run` from `firmware/`, which must succeed with `[SUCCESS]`. Final manual hardware verification (Task 5) confirms the visuals actually look right, and is also where the user's real icon artwork gets swapped in once supplied.

---

### Task 1: Add 4 new weather-condition icon slots

**Files:**
- Modify: `firmware/src/display_manager.h`
- Modify: `firmware/src/display_manager.cpp`

- [ ] **Step 1: Add 4 new Icon enum values**

In `firmware/src/display_manager.h`, find:

```cpp
    ICON_MEASURE_LG,
    ICON_TIME_LG,
    ICON_WEATHER_LG,
    ICON_LOCATE_LG,
```

Replace it with:

```cpp
    ICON_MEASURE_LG,
    ICON_TIME_LG,
    ICON_WEATHER_LG,
    ICON_WEATHER_SUN_LG,
    ICON_WEATHER_RAIN_LG,
    ICON_WEATHER_STORM_LG,
    ICON_WEATHER_SNOW_LG,
    ICON_LOCATE_LG,
```

- [ ] **Step 2: Add 4 placeholder XBM arrays**

In `firmware/src/display_manager.cpp`, find:

```cpp
// weather: sun with cloud (unchanged)
static const uint8_t xbm_weather_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x30, 0x18, 0x0c, 0x70, 
	0x00, 0x0e, 0x60, 0x3c, 0x06, 0x00, 0xff, 0x00, 0x80, 0xc3, 0x01, 0x80, 0x81, 0x01, 0xc0, 0x00, 
	0x03, 0xf0, 0x00, 0x7b, 0xf8, 0x01, 0x7b, 0x1c, 0x03, 0x03, 0x0e, 0x8e, 0x01, 0x06, 0xde, 0x01, 
	0x06, 0xf0, 0x00, 0x0e, 0x30, 0x06, 0x1c, 0x30, 0x0e, 0xf8, 0x1f, 0x0c, 0xf0, 0x0f, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// locate: map pin
```

Replace it with:

```cpp
// weather: sun with cloud (unchanged)
static const uint8_t xbm_weather_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x30, 0x18, 0x0c, 0x70, 
	0x00, 0x0e, 0x60, 0x3c, 0x06, 0x00, 0xff, 0x00, 0x80, 0xc3, 0x01, 0x80, 0x81, 0x01, 0xc0, 0x00, 
	0x03, 0xf0, 0x00, 0x7b, 0xf8, 0x01, 0x7b, 0x1c, 0x03, 0x03, 0x0e, 0x8e, 0x01, 0x06, 0xde, 0x01, 
	0x06, 0xf0, 0x00, 0x0e, 0x30, 0x06, 0x1c, 0x30, 0x0e, 0xf8, 0x1f, 0x0c, 0xf0, 0x0f, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// weather: clear sun -- PLACEHOLDER (all-zero, renders blank). Replace with
// real 24x24 XBM byte data once sourced (same image2cpp workflow used for
// the Time menu icon). Must stay exactly 72 bytes (3 bytes/row x 24 rows).
static const uint8_t xbm_weather_sun_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};
// weather: rain -- PLACEHOLDER (all-zero, renders blank). Replace with real
// 24x24 XBM byte data once sourced. Must stay exactly 72 bytes.
static const uint8_t xbm_weather_rain_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};
// weather: thunderstorm -- PLACEHOLDER (all-zero, renders blank). Replace
// with real 24x24 XBM byte data once sourced. Must stay exactly 72 bytes.
static const uint8_t xbm_weather_storm_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};
// weather: snow -- PLACEHOLDER (all-zero, renders blank). Replace with real
// 24x24 XBM byte data once sourced. Must stay exactly 72 bytes.
static const uint8_t xbm_weather_snow_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
};
// locate: map pin
```

- [ ] **Step 3: Add 4 new switch cases**

Find:

```cpp
        case dm::ICON_MEASURE_LG:  return xbm_measure_lg;
        case dm::ICON_TIME_LG:     return xbm_time_lg;
        case dm::ICON_WEATHER_LG:  return xbm_weather_lg;
        case dm::ICON_LOCATE_LG:   return xbm_locate_lg;
```

Replace it with:

```cpp
        case dm::ICON_MEASURE_LG:  return xbm_measure_lg;
        case dm::ICON_TIME_LG:     return xbm_time_lg;
        case dm::ICON_WEATHER_LG:  return xbm_weather_lg;
        case dm::ICON_WEATHER_SUN_LG:   return xbm_weather_sun_lg;
        case dm::ICON_WEATHER_RAIN_LG:  return xbm_weather_rain_lg;
        case dm::ICON_WEATHER_STORM_LG: return xbm_weather_storm_lg;
        case dm::ICON_WEATHER_SNOW_LG:  return xbm_weather_snow_lg;
        case dm::ICON_LOCATE_LG:   return xbm_locate_lg;
```

- [ ] **Step 4: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`. The 4 new icons will render as blank squares until real icon data is supplied — that's expected at this stage, not a bug.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/display_manager.h firmware/src/display_manager.cpp
git commit -m "feat(firmware): add 4 weather-condition icon slots (placeholder art)

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Add weather-condition-to-icon mapping

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Add the resolveWeatherIcon() function**

In `firmware/src/main.cpp`, find the forward declaration:

```cpp
static void drawWeatherScreen(float tempC, int humPct, const char *desc);
```

Replace it with:

```cpp
static void drawWeatherScreen(float tempC, int humPct, dm::Icon conditionIcon);

// Maps OpenWeatherMap's weather[0].main field to an icon. That field is
// always one of a fixed set of capitalized-mixed-case strings (see
// https://openweathermap.org/weather-conditions): Thunderstorm, Drizzle,
// Rain, Snow, Clear, Clouds, Mist, Smoke, Haze, Dust, Fog, Sand, Ash,
// Squall, Tornado. Anything not explicitly mapped below falls back to the
// existing generic cloud icon.
static dm::Icon resolveWeatherIcon(const String &main) {
  if (main == "Clear") return dm::ICON_WEATHER_SUN_LG;
  if (main == "Rain" || main == "Drizzle") return dm::ICON_WEATHER_RAIN_LG;
  if (main == "Thunderstorm") return dm::ICON_WEATHER_STORM_LG;
  if (main == "Snow") return dm::ICON_WEATHER_SNOW_LG;
  return dm::ICON_WEATHER_LG; // Clouds, Mist, Haze, Fog, etc.
}
```

- [ ] **Step 2: Update showWeatherPage()'s call site**

Find:

```cpp
    if (code == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      float temp = doc["main"]["temp"].as<float>();
      int hum = doc["main"]["humidity"].as<int>();
      String desc = doc["weather"][0]["main"].as<String>();
      desc.toUpperCase();
      drawWeatherScreen(temp, hum, desc.c_str());
    } else {
```

Replace it with:

```cpp
    if (code == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      float temp = doc["main"]["temp"].as<float>();
      int hum = doc["main"]["humidity"].as<int>();
      String main = doc["weather"][0]["main"].as<String>();
      drawWeatherScreen(temp, hum, resolveWeatherIcon(main));
    } else {
```

Find the `#else` fallback branch (used when `WEATHER_API_KEY` isn't defined):

```cpp
#else
  drawWeatherScreen(30.5f, 68, "SUNNY");
#endif
```

Replace it with:

```cpp
#else
  drawWeatherScreen(30.5f, 68, dm::ICON_WEATHER_SUN_LG);
#endif
```

- [ ] **Step 3: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: this will FAIL — `drawWeatherScreen()`'s definition (not yet updated, that's Task 3) still has the old `const char *desc` signature, so the new call sites passing a `dm::Icon` won't match. Confirm the error is specifically a signature/argument-type mismatch on `drawWeatherScreen`, not something else. This is expected; Task 3 fixes it.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): add weather condition-to-icon mapping

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Redesign drawWeatherScreen() — icon left, stats stacked right

**Files:**
- Modify: `firmware/src/main.cpp`

This task makes the build pass again (fixes the signature mismatch introduced in Task 2).

- [ ] **Step 1: Replace drawWeatherScreen()**

Find:

```cpp
static void drawWeatherScreen(float tempC, int humPct, const char *desc) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "WEATHER", false, dm::ICON_WIFI);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%d", tInt);
  snprintf(humBuf,  sizeof(humBuf),  "%d", humPct);

  // Two columns, unit lives in the label row so nothing collides horizontally.
  drawColumnValue(OLED_OFFSET_X + 2, 28, OLED_OFFSET_Y + 11, "TEMP C", tempBuf);
  dm::drawVLine(OLED_OFFSET_X + 32, OLED_OFFSET_Y + 11, 26);
  drawColumnValue(OLED_OFFSET_X + 34, 28, OLED_OFFSET_Y + 11, "HUM %", humBuf);

  // Footer inverted bar with condition text.
  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  int dw = dm::textWidth(desc);
  int dx = OLED_OFFSET_X + (OLED_W - dw) / 2;
  if (dx < 2) dx = 2;
  dm::drawTextInverted(dx, OLED_OFFSET_Y + OLED_H - 8, desc);
  dm::endFrame();
}
```

Replace it with:

```cpp
static void drawWeatherScreen(float tempC, int humPct, dm::Icon conditionIcon) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "WEATHER", false, dm::ICON_WIFI);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%dC", tInt);
  snprintf(humBuf,  sizeof(humBuf),  "%d%%", humPct);

  // Icon left, vertically centred in the content area (y=12..48, 36px tall,
  // no footer on this screen -- the icon now conveys condition instead of
  // the old inverted-bar text label).
  int iconX = OLED_OFFSET_X + 3;
  int iconY = OLED_OFFSET_Y + 12 + (36 - 24) / 2;
  dm::drawIcon24(iconX, iconY, conditionIcon);

  // Stats stacked right: hero temp line, secondary humidity line below it.
  // tempBuf is always exactly 3 chars ("-9C".."99C" given the clamp above),
  // so FONT_LARGE (~10px/char) fits comfortably in the ~32px right column.
  int rightX = OLED_OFFSET_X + 32;
  dm::setFont(dm::FONT_LARGE);
  dm::drawText(rightX, OLED_OFFSET_Y + 32, tempBuf);
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(rightX, OLED_OFFSET_Y + 44, humBuf);

  dm::endFrame();
}
```

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): redesign Weather screen to icon-left layout

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Redesign drawMeasureSample() — icon left, stats stacked right, keep LDR footer

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Replace drawMeasureSample()**

Find:

```cpp
static void drawMeasureSample(int sampleIdx, int totalSamples,
                              float tempC, int humPct, int ldr) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  char headBuf[16];
  snprintf(headBuf, sizeof(headBuf), "SCAN %d/%d", sampleIdx, totalSamples);
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, headBuf, true, dm::ICON_SCAN);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%d", tInt);
  snprintf(humBuf,  sizeof(humBuf),  "%d", humPct);

  drawColumnValue(OLED_OFFSET_X + 2,  28, OLED_OFFSET_Y + 11, "TEMP C", tempBuf);
  dm::drawVLine(OLED_OFFSET_X + 32, OLED_OFFSET_Y + 11, 26);
  drawColumnValue(OLED_OFFSET_X + 34, 28, OLED_OFFSET_Y + 11, "HUM %", humBuf);

  // Footer: LDR value in inverted bar.
  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  char lb[12]; snprintf(lb, sizeof(lb), "LDR %d", ldr);
  int lw = dm::textWidth(lb);
  int lx = OLED_OFFSET_X + (OLED_W - lw) / 2;
  if (lx < 2) lx = 2;
  dm::drawTextInverted(lx, OLED_OFFSET_Y + OLED_H - 8, lb);
  dm::endFrame();
}
```

Replace it with:

```cpp
static void drawMeasureSample(int sampleIdx, int totalSamples,
                              float tempC, int humPct, int ldr) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  char headBuf[16];
  snprintf(headBuf, sizeof(headBuf), "SCAN %d/%d", sampleIdx, totalSamples);
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, headBuf, true, dm::ICON_SCAN);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%dC", tInt);
  snprintf(humBuf,  sizeof(humBuf),  "%d%%", humPct);

  // Icon left, vertically centred in the content area (y=12..39, 27px tall
  // -- shorter than Weather's since this screen keeps the LDR footer).
  int iconX = OLED_OFFSET_X + 3;
  int iconY = OLED_OFFSET_Y + 12 + (27 - 24) / 2;
  dm::drawIcon24(iconX, iconY, dm::ICON_MEASURE_LG);

  // Stats stacked right: hero temp line, secondary humidity line below it.
  int rightX = OLED_OFFSET_X + 32;
  dm::setFont(dm::FONT_LARGE);
  dm::drawText(rightX, OLED_OFFSET_Y + 27, tempBuf);
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(rightX, OLED_OFFSET_Y + 37, humBuf);

  // Footer: LDR value in inverted bar (unchanged from before).
  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  char lb[12]; snprintf(lb, sizeof(lb), "LDR %d", ldr);
  int lw = dm::textWidth(lb);
  int lx = OLED_OFFSET_X + (OLED_W - lw) / 2;
  if (lx < 2) lx = 2;
  dm::drawTextInverted(lx, OLED_OFFSET_Y + OLED_H - 8, lb);
  dm::endFrame();
}
```

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): redesign Measure scan screen to icon-left layout

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Manual hardware verification (and swap in real icon artwork when ready)

**Files:** none from the agent — the user supplies 4 icon byte arrays when ready (see Step 1)

- [ ] **Step 1: Swap in real weather-condition icons once sourced**

When the user has converted sun/rain/storm/snow artwork to 24x24 XBM (same
image2cpp workflow used for the Time menu icon earlier), replace the
placeholder all-zero arrays in `firmware/src/display_manager.cpp`:
- `xbm_weather_sun_lg[]`
- `xbm_weather_rain_lg[]`
- `xbm_weather_storm_lg[]`
- `xbm_weather_snow_lg[]`

Each must stay exactly 72 bytes (3 bytes/row x 24 rows, LSB-first). No code
changes needed beyond replacing the byte contents — the enum, switch cases,
and mapping function from Tasks 1-2 already reference these array names.
Rebuild (`pio run`) after swapping to confirm it still compiles.

- [ ] **Step 2: Flash the firmware**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload`

- [ ] **Step 3: Verify the Weather screen**

Trigger Weather from the menu. Confirm:
- Icon renders on the left (blank square if real art hasn't been swapped in
  yet — that's expected until Step 1 is done).
- Temperature shows as the larger "hero" number with a trailing `C` (e.g.
  `24C`), no overlap with the icon or screen edges.
- Humidity shows as a smaller line below the temperature (e.g. `55%`).
- No condition text/footer bar is shown (removed by design).
- Try to observe at least one non-`Clouds` condition if possible (e.g. on a
  clear day) to confirm `resolveWeatherIcon()` picks the right icon.

- [ ] **Step 4: Verify the Measure scan screen**

Trigger Measure from the menu. During the 5 samples, confirm:
- Thermometer icon renders on the left, doesn't overlap the header or the
  temp/humidity text.
- Temperature and humidity render correctly stacked on the right.
- LDR value still shows correctly in the footer bar exactly as before.

- [ ] **Step 5: Confirm no regressions to untouched screens**

Quickly check that Stats (`drawStatsScreen()`, untouched, still uses the old
two-column layout by design) and the Measure glitch/error screens still look
and behave exactly as before.

This step requires physical device access and cannot be automated — mark it
done only after actually observing the behavior on hardware.
