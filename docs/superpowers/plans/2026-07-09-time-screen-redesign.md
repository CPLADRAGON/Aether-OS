# Time Screen Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the Time screen with a procedural sun/moon icon, a header-less "huge time" layout, and a proportioned day/date info strip, per the approved browser-mockup design.

**Architecture:** `drawClockScreen()` gains one new parameter (`hour24`) to decide sun vs. moon, and its internal layout is rewritten (icon row, then time, then divider, then info strip, no header bar). `showTimePage()`'s call site passes the already-available `tinfo.tm_hour` through and switches its date format string.

**Tech Stack:** PlatformIO / Arduino, ESP32, U8g2 (64x48 monochrome OLED).

**Spec:** `docs/superpowers/specs/2026-07-09-time-screen-redesign-design.md`

**Verification:** No test runner exists for this firmware project — every task's verification step is `pio run` from `firmware/`, which must succeed with `[SUCCESS]`. Final manual hardware verification (Task 3) confirms the visuals actually look right and the sun/moon icon doesn't collide with the time digits, since exact font metrics are approximate in this codebase.

---

### Task 1: Redesign drawClockScreen()

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Update the forward declaration**

Find:

```cpp
static void drawClockScreen(const char *hh, const char *mm, const char *ss,
                            const char *ddmmyy, const char *day);
```

Replace it with:

```cpp
static void drawClockScreen(const char *hh, const char *mm, const char *ss,
                            const char *ddmmm, const char *day, int hour24);
```

- [ ] **Step 2: Replace the drawClockScreen() definition**

Find:

```cpp
static void drawClockScreen(const char *hh, const char *mm, const char *ss,
                            const char *ddmmyy, const char *day) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, day, false, dm::ICON_WIFI);

  // Big HH:MM centred vertically in the body area. HH, ":", and MM are drawn
  // as three independent calls at FIXED cumulative offsets — this guarantees
  // MM's x position never depends on whether the colon is currently visible.
  // (A previous version swapped ':' for ' ' in a combined string, but ' ' and
  // ':' have different glyph advance widths in this numeric font, which
  // shifted MM sideways every time the colon blinked.)
  dm::setFont(dm::FONT_HUGE);
  int hhW = dm::textWidth(hh);
  int colonW = dm::textWidth(":");
  int mmW = dm::textWidth(mm);
  int totalW = hhW + colonW + mmW;
  int x = OLED_OFFSET_X + (OLED_W - totalW) / 2;
  if (x < 0) x = 0;

  int secInt = (ss && ss[0] && ss[1]) ? ((ss[0]-'0')*10 + (ss[1]-'0')) : 0;

  dm::drawText(x, OLED_OFFSET_Y + 12, hh);
  if (!(secInt & 1)) {
    dm::drawText(x + hhW, OLED_OFFSET_Y + 12, ":");
  }
  dm::drawText(x + hhW + colonW, OLED_OFFSET_Y + 12, mm);  // occupies y=12..30

  // Footer inverted bar: date on left, ticking seconds on right.
  // Bar top at y=39 keeps a 8-px gap under the digits (which end at y=30).
  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  dm::drawTextInverted(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, ddmmyy);
  int sw = dm::textWidth(ss);
  dm::drawTextInverted(OLED_OFFSET_X + OLED_W - sw - 2, OLED_OFFSET_Y + OLED_H - 8, ss);

  // Seconds progress bar just above the footer (y=35..36), safely below the
  // digits' descenders (which end at y=31).
  int barX = OLED_OFFSET_X + 4;
  int barY = OLED_OFFSET_Y + 35;
  int barMaxW = OLED_W - 8;
  int barFill = (barMaxW * secInt) / 60;
  dm::drawHLine(barX, barY + 1, barMaxW);
  if (barFill > 0) dm::drawFilledRect(barX, barY, barFill, 2);
  dm::endFrame();
}
```

Replace it with:

```cpp
static void drawClockScreen(const char *hh, const char *mm, const char *ss,
                            const char *ddmmm, const char *day, int hour24) {
  if (!dm::beginFrame(portMAX_DELAY)) return;

  // No header bar on this screen (deliberate exception to the convention
  // every other screen follows) -- day/date now live in the info strip at
  // the bottom instead, freeing the full 48px height for the huge time.

  // Sun/moon icon in its own small reserved row at the very top, drawn
  // procedurally (no bitmap asset needed) with the same circle-primitive
  // technique already used for the sleep animation's crescent moon. Placed
  // in a row ABOVE the time rather than beside it -- sharing a row with the
  // huge time digits risked colliding with the rightmost digit, since this
  // font runs close to the full 64px width when centred.
  int sunMoonCx = OLED_OFFSET_X + OLED_W - 7;
  int sunMoonCy = OLED_OFFSET_Y + 4;
  bool isDaytime = (hour24 >= 6 && hour24 < 18);
  if (isDaytime) {
    // Sun: filled circle + 8 short rays.
    dm::drawFilledCircle(sunMoonCx, sunMoonCy, 2);
    for (int i = 0; i < 8; i++) {
      float ang = (float)i * (3.14159265f / 4.0f);
      int x0 = sunMoonCx + (int)(cosf(ang) * 3);
      int y0 = sunMoonCy + (int)(sinf(ang) * 3);
      int x1 = sunMoonCx + (int)(cosf(ang) * 4);
      int y1 = sunMoonCy + (int)(sinf(ang) * 4);
      dm::drawLine(x0, y0, x1, y1);
    }
  } else {
    // Moon: filled circle with an offset circle carved out (background
    // colour) to create the crescent shape.
    dm::drawFilledCircle(sunMoonCx, sunMoonCy, 3);
    dm::clearCircle(sunMoonCx + 1, sunMoonCy - 1, 2);
  }

  // Big HH:MM. HH, ":", and MM are drawn as three independent calls at FIXED
  // cumulative offsets — this guarantees MM's x position never depends on
  // whether the colon is currently visible. (A previous version swapped ':'
  // for ' ' in a combined string, but ' ' and ':' have different glyph
  // advance widths in this numeric font, which shifted MM sideways every
  // time the colon blinked.)
  dm::setFont(dm::FONT_HUGE);
  int hhW = dm::textWidth(hh);
  int colonW = dm::textWidth(":");
  int mmW = dm::textWidth(mm);
  int totalW = hhW + colonW + mmW;
  int x = OLED_OFFSET_X + (OLED_W - totalW) / 2;
  if (x < 0) x = 0;

  int secInt = (ss && ss[0] && ss[1]) ? ((ss[0]-'0')*10 + (ss[1]-'0')) : 0;

  dm::drawText(x, OLED_OFFSET_Y + 8, hh);
  if (!(secInt & 1)) {
    dm::drawText(x + hhW, OLED_OFFSET_Y + 8, ":");
  }
  dm::drawText(x + hhW + colonW, OLED_OFFSET_Y + 8, mm);  // occupies y=8..36

  // Thin divider separating the huge time from the info strip below.
  dm::drawHLine(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 37, OLED_W - 4);

  // Info strip: day left, date right -- proportioned (FONT_SMALL, not tiny),
  // a Pebble/G-Shock-watch-face-style treatment rather than an afterthought
  // footer. No seconds shown anywhere in this design (colon still blinks
  // using secInt internally, it's just not rendered as a visible number).
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 39, day);
  int dw = dm::textWidth(ddmmm);
  dm::drawText(OLED_OFFSET_X + OLED_W - dw - 2, OLED_OFFSET_Y + 39, ddmmm);

  dm::endFrame();
}
```

- [ ] **Step 3: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: this WILL FAIL — `showTimePage()` (not yet updated, that's the next task) still calls `drawClockScreen(hh, mm, ss, dStr, day.c_str())` with the old 5-argument signature (missing the new `hour24` parameter). Confirm the error is specifically about `drawClockScreen`'s argument count not matching, not something else. This is expected; the next task fixes it.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): redesign Time screen with sun/moon icon and info strip

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Update showTimePage()'s call site

**Files:**
- Modify: `firmware/src/main.cpp`

This task makes the build pass again (fixes the call-site mismatch from Task 1).

- [ ] **Step 1: Update the date format and call site**

Find:

```cpp
  while (millis() - start < 8000) {
    if (getLocalTime(&tinfo)) {
      char hh[3], mm[3], ss[4], dStr[12], dayStr[16];
      strftime(hh,   sizeof(hh),   "%H", &tinfo);
      strftime(mm,   sizeof(mm),   "%M", &tinfo);
      strftime(ss,   sizeof(ss),   "%S", &tinfo);
      strftime(dStr, sizeof(dStr), "%d/%m", &tinfo);
      strftime(dayStr, sizeof(dayStr), "%a", &tinfo);
      String day = String(dayStr);
      day.toUpperCase();
      drawClockScreen(hh, mm, ss, dStr, day.c_str());
    }
    if (waitWithButtonPoll(500))
      break; // Cancel on button press
  }
```

Replace it with:

```cpp
  while (millis() - start < 8000) {
    if (getLocalTime(&tinfo)) {
      char hh[3], mm[3], ss[4], dStr[12], dayStr[16];
      strftime(hh,   sizeof(hh),   "%H", &tinfo);
      strftime(mm,   sizeof(mm),   "%M", &tinfo);
      strftime(ss,   sizeof(ss),   "%S", &tinfo);
      strftime(dStr, sizeof(dStr), "%d %b", &tinfo);
      strftime(dayStr, sizeof(dayStr), "%a", &tinfo);
      String day = String(dayStr);
      day.toUpperCase();
      String dateStr = String(dStr);
      dateStr.toUpperCase();
      drawClockScreen(hh, mm, ss, dateStr.c_str(), day.c_str(), tinfo.tm_hour);
    }
    if (waitWithButtonPoll(500))
      break; // Cancel on button press
  }
```

Note: `dStr` (from `strftime(..., "%d %b", ...)`, e.g. "09 Jul") needs uppercasing
to "09 JUL" to match the existing day-name uppercasing convention -- `strftime`
doesn't have a built-in uppercase option, hence the extra `String`/`toUpperCase()`
step, mirroring exactly how `day` is already uppercased two lines above it.
`tinfo.tm_hour` is the `struct tm`'s existing 24-hour hour field (0-23), already
populated by the `getLocalTime(&tinfo)` call directly above -- no new time-fetching
logic needed.

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): pass hour and friendlier date format to Time screen

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Manual hardware verification

**Files:** none (verification only)

- [ ] **Step 1: Flash the firmware**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload`

- [ ] **Step 2: Verify the layout**

Navigate to Time from the menu. Confirm:
- No header bar is shown (screen starts directly with the icon row).
- Sun or moon icon shows in the top area, matching the current time of day
  (sun if it's currently between 6am and 6pm, moon otherwise) — you may need
  to trust the logic if it's not convenient to test both at the actual time
  of testing, but if possible check the icon switches correctly around the
  6am/6pm boundaries.
- Icon does NOT overlap or collide with the huge HH:MM digits below it.
- HH:MM digits are centred, colon blinks every other second, and MM does NOT
  shift position when the colon toggles on/off (this was a previously-fixed
  bug — confirm it's still fixed with the new layout).
- Thin divider line shows between the time and the info strip.
- Info strip shows day (e.g. "MON") on the left and date (e.g. "09 JUL") on
  the right, both readable, no clipping at the screen edges.

- [ ] **Step 3: Adjust pixel positions if needed**

Given this codebase's font-height comments are approximate (not exact pixel
measurements), if anything overlaps or looks cramped on real hardware, nudge
the y-values in `drawClockScreen()` (the icon row's `sunMoonCy`, the time's
`OLED_OFFSET_Y + 8`, the divider's `OLED_OFFSET_Y + 37`, or the info strip's
`OLED_OFFSET_Y + 39`) by a few pixels and rebuild/reflash to confirm the fix.

- [ ] **Step 4: Confirm no regressions elsewhere**

Quickly check that other screens (Weather, Measure, Room) still look and
behave exactly as before — this change only touched Time-screen-specific code.

This step requires physical device access and cannot be automated — mark it
done only after actually observing the behavior on hardware.
