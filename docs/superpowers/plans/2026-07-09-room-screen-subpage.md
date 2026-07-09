# Room Screen Subpage Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Free the Room screen's main view to use Weather's full-height layout by moving its comfort-tag footer content to a new short-tap-accessible subpage with a richer temp/humidity/light breakdown.

**Architecture:** A new button-wait helper distinguishes short tap from long press as discrete events (reusing the existing `isPressing`/`isrPressStart`/`LONG_PRESS_MS` primitives already used elsewhere in this firmware). `showRoomPage()` becomes a small state loop toggling between the restyled main view and a new subpage renderer on each short tap, exiting to the menu on long press or timeout.

**Tech Stack:** PlatformIO / Arduino, ESP32, U8g2 (64x48 monochrome OLED).

**Spec:** `docs/superpowers/specs/2026-07-09-room-screen-subpage-design.md`

**Verification:** No test runner exists for this firmware project — every task's verification step is `pio run` from `firmware/`, which must succeed with `[SUCCESS]`. Final manual hardware verification (Task 4) confirms the interaction actually works, since button-press timing/behavior cannot be verified from a build alone.

---

### Task 1: Add waitRoomInteraction() short-tap/long-press helper

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Add the helper function next to the existing waitWithButtonPoll()**

Find:

```cpp
bool waitWithButtonPoll(unsigned long ms) {
  buttonEvent = false; // Reset before waiting
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (buttonEvent) {
      buttonEvent = false;
      lastInteractionTime = millis();
      return true;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  return false;
}
```

Replace it with:

```cpp
bool waitWithButtonPoll(unsigned long ms) {
  buttonEvent = false; // Reset before waiting
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (buttonEvent) {
      buttonEvent = false;
      lastInteractionTime = millis();
      return true;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  return false;
}

// Waits for either a completed short tap or a long press, distinguishing
// them as discrete events (unlike waitWithButtonPoll(), which treats any
// button activity the same). Used by showRoomPage() to toggle between its
// main view and status subpage on short tap, while still exiting instantly
// on long press (checked continuously while held, matching the existing
// "check for long press to exit instantly" pattern used elsewhere in this
// file, e.g. in runMeasurementFlow()).
//
// Returns true if any interaction occurred (and sets *longPress
// accordingly), or false on timeout with no interaction at all.
bool waitRoomInteraction(unsigned long ms, bool *longPress) {
  *longPress = false;
  buttonEvent = false;
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
      *longPress = true;
      lastInteractionTime = millis();
      return true;
    }
    if (buttonEvent) {
      buttonEvent = false;
      lastInteractionTime = millis();
      return true; // short tap
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
  return false;
}
```

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]` — this is additive (no call sites yet), nothing else should change.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): add waitRoomInteraction short-tap/long-press helper

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Restyle drawRoomStatus() to Weather's layout + add drawRoomStatusDetail() subpage

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Replace drawRoomStatus() and add drawRoomStatusDetail() after it**

Find:

```cpp
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
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%dC", tInt);
  snprintf(humBuf, sizeof(humBuf), "%d%%", humPct);

  // Icon left, vertically centred in the content area (y=12..39, 27px tall
  // -- same tight budget as the Measure scan screen since this screen also
  // keeps a footer (comfort tag) below). Reuses Measure's already
  // pixel-verified font choices/y-values: FONT_NORMAL hero + FONT_SMALL
  // secondary, both drawn top-anchored (setFontPosTop()) and clear of the
  // footer starting at y=39.
  int iconX = OLED_OFFSET_X + 3;
  int iconY = OLED_OFFSET_Y + 12 + (27 - 24) / 2;
  dm::drawIcon24(iconX, iconY, dm::ICON_ROOM_LG);

  int rightX = OLED_OFFSET_X + 32;
  dm::setFont(dm::FONT_NORMAL);
  dm::drawText(rightX, OLED_OFFSET_Y + 13, tempBuf);
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(rightX, OLED_OFFSET_Y + 25, humBuf);

  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  int cw = dm::textWidth(comfortTag);
  int cx = OLED_OFFSET_X + (OLED_W - cw) / 2;
  if (cx < 2) cx = 2;
  dm::drawTextInverted(cx, OLED_OFFSET_Y + OLED_H - 8, comfortTag);
  dm::endFrame();
}
```

Replace it with:

```cpp
// ROOM: instant local DHT11+LDR snapshot, no WiFi/upload, no history append
// (kept separate from the quality-checked 5-sample MEASURE average).
//
// Main view -- matches drawWeatherScreen()'s exact layout (icon left, hero
// temp + secondary humidity right, full 36px content area, no footer). The
// comfort tag that used to live in a footer here has moved to
// drawRoomStatusDetail() (the "Room Status" subpage, reached by short tap);
// a small ">" chevron in the bottom-right corner hints that it's available.
static void drawRoomStatus(float tempC, int humPct, int ldrRaw,
                           const char *lightTag) {
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
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%dC", tInt);
  snprintf(humBuf, sizeof(humBuf), "%d%%", humPct);

  // Icon left, vertically centred in the full 36px content area (y=12..47,
  // no footer) -- same positions as drawWeatherScreen() since the content
  // area is now identically sized.
  int iconX = OLED_OFFSET_X + 3;
  int iconY = OLED_OFFSET_Y + 12 + (36 - 24) / 2;
  dm::drawIcon24(iconX, iconY, dm::ICON_ROOM_LG);

  int rightX = OLED_OFFSET_X + 32;
  dm::setFont(dm::FONT_LARGE);
  dm::drawText(rightX, OLED_OFFSET_Y + 14, tempBuf);
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(rightX, OLED_OFFSET_Y + 36, humBuf);

  // Chevron indicator hinting at the Room Status subpage (short tap toggles
  // to it). Uses plain ">" rather than a real "▸" glyph -- this codebase
  // avoids non-ASCII characters in OLED text (same reasoning as avoiding a
  // real "°" degree symbol elsewhere: no confirmed glyph support in this
  // font, ASCII is a safe bet).
  dm::setFont(dm::FONT_SMALL);
  const char *chevron = ">";
  int chW = dm::textWidth(chevron);
  dm::drawText(OLED_OFFSET_X + OLED_W - chW - 3, OLED_OFFSET_Y + OLED_H - 9, chevron);

  dm::endFrame();
}

// ROOM STATUS subpage: richer breakdown of temp/humidity/light, each as a
// tag + raw value on its own line. Reached by short-tapping the ROOM main
// view; short tap again returns to it (see showRoomPage()).
static void drawRoomStatusDetail(int tempInt, const char *tempTag,
                                 int humPct, const char *humTag,
                                 int ldrRaw, const char *lightTag) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "STATUS", false, dm::ICON_WIFI);

  dm::setFont(dm::FONT_SMALL);

  char tLine[20], hLine[20], lLine[20];
  snprintf(tLine, sizeof(tLine), "T:%s %dC", tempTag, tempInt);
  snprintf(hLine, sizeof(hLine), "H:%s %d%%", humTag, humPct);
  snprintf(lLine, sizeof(lLine), "L:%s %d", lightTag, ldrRaw);

  // Guard against the rare case where a long tag + value combination would
  // overflow the 64px panel width (e.g. "L:BRIGHT 4095", a 4-digit LDR
  // reading) -- drop the raw value for that specific line rather than let
  // it clip off-screen. Matches the existing measure-then-adapt pattern
  // already used elsewhere in this file (see drawColumnValue()).
  if (dm::textWidth(tLine) > OLED_W - 4) snprintf(tLine, sizeof(tLine), "T:%s", tempTag);
  if (dm::textWidth(hLine) > OLED_W - 4) snprintf(hLine, sizeof(hLine), "H:%s", humTag);
  if (dm::textWidth(lLine) > OLED_W - 4) snprintf(lLine, sizeof(lLine), "L:%s", lightTag);

  dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 16, tLine);
  dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 26, hLine);
  dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 36, lLine);

  dm::endFrame();
}
```

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: this WILL FAIL — `showRoomPage()` (not yet updated, that's the next task) still calls `drawRoomStatus(t, hInt, ldrRaw, comfortTag, lightTag)` with the old 5-argument signature (including `comfortTag`, which the new signature no longer accepts). Confirm the error is specifically about `drawRoomStatus`'s argument count/types not matching, not something else. This is expected; the next task fixes it.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): restyle Room main view to Weather's layout, add Status subpage

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Update showRoomPage() to toggle between main view and subpage

**Files:**
- Modify: `firmware/src/main.cpp`

This task makes the build pass again (fixes the call-site mismatch from Task 2).

- [ ] **Step 1: Replace showRoomPage()**

Find:

```cpp
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

  // NOTE: this LDR's voltage-divider wiring reads LOW when the room is
  // bright and HIGH when dark (verified on hardware) — the opposite of the
  // "higher = brighter" assumption a photoresistor-on-top divider would give.
  const char *lightTag = (ldrRaw < ROOM_LDR_BRIGHT_MAX)   ? "BRIGHT"
                        : (ldrRaw < ROOM_LDR_DARK_MIN)     ? "DIM"
                                                             : "DARK";

  drawRoomStatus(t, hInt, ldrRaw, comfortTag, lightTag);
  waitWithButtonPoll(5000);
}
```

Replace it with:

```cpp
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

  // NOTE: this LDR's voltage-divider wiring reads LOW when the room is
  // bright and HIGH when dark (verified on hardware) — the opposite of the
  // "higher = brighter" assumption a photoresistor-on-top divider would give.
  const char *lightTag = (ldrRaw < ROOM_LDR_BRIGHT_MAX)   ? "BRIGHT"
                        : (ldrRaw < ROOM_LDR_DARK_MIN)     ? "DIM"
                                                             : "DARK";

  // Toggle loop: short tap switches between the main view and the Status
  // subpage; long press or timeout exits to the menu. Exactly two views,
  // no deeper navigation stack.
  bool showingDetail = false;
  while (true) {
    if (showingDetail) {
      drawRoomStatusDetail(tInt, tempTag, hInt, humTag, ldrRaw, lightTag);
    } else {
      drawRoomStatus(t, hInt, ldrRaw, lightTag);
    }

    bool longPress = false;
    if (!waitRoomInteraction(5000, &longPress)) {
      return; // timeout -> back to menu
    }
    if (longPress) {
      return; // long press -> back to menu
    }
    showingDetail = !showingDetail; // short tap -> toggle view
  }
}
```

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): wire up Room main view <-> Status subpage toggle

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Manual hardware verification

**Files:** none (verification only)

- [ ] **Step 1: Flash the firmware**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload`

- [ ] **Step 2: Verify the Room main view**

Navigate to Room from the menu. Confirm:
- Layout matches Weather: house icon left, big temperature on the right, humidity below it, no footer bar.
- The light-level tag (BRIGHT/DIM/DARK) still shows in the header's top-right corner, unchanged.
- A small ">" chevron shows in the bottom-right corner.

- [ ] **Step 3: Verify the toggle to/from the Status subpage**

Short-tap while on the Room main view. Confirm:
- Screen switches to "STATUS" header with three lines: temp tag + value, humidity tag + value, light tag + value (e.g. "T:WARM 24C", "H:NORMAL 52%", "L:BRIGHT 812"), no overlap or clipping.
- Short-tap again switches back to the Room main view.
- Repeat a few times to confirm it reliably toggles back and forth (not just once).

- [ ] **Step 4: Verify long press and timeout still exit to the menu**

From the Room main view, long-press (hold ≥1.2s) — confirm it exits directly to the main menu (not to the subpage). Repeat from the Status subpage — confirm long press from there also exits to the menu, not back to the main view first. Separately, navigate to Room and simply wait ~5+ seconds without pressing anything — confirm it times out back to the menu, same as before this change.

- [ ] **Step 5: Confirm no regressions elsewhere**

Quickly check Weather and Measure screens (whose layout code this Room change reused but did not modify) still look and behave exactly as before.

This step requires physical device access and cannot be automated — mark it done only after actually observing the behavior on hardware.
