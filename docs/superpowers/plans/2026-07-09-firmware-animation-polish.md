# Firmware Animation Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace three weak/mechanical firmware animations (deep-sleep shutdown, menu cover-flow, WiFi-connecting) with the concepts approved during brainstorming — moon & stars settle, scale-depth carousel, and signal-strength search — while leaving the already-good toast and other connecting-state animations untouched.

**Architecture:** Two new small rendering primitives (`drawIconScaled`, `drawArcUpperHalf`) are added to `display_manager` first, since both later tasks depend on them. Then each animation site in `main.cpp` / `display_manager.cpp` is updated to use the new visual concept, reusing the existing timing patterns already established in the codebase (blocking frame loops for one-shot sequences, `nowMs`-based stepping for looping state animations).

**Tech Stack:** PlatformIO / Arduino, ESP32, U8g2 (64x48 monochrome OLED).

**Spec:** `docs/superpowers/specs/2026-07-09-firmware-animation-polish-design.md`

**Verification:** No test runner exists for this firmware project (per project conventions) — every task's verification step is `pio run` from the `firmware/` directory, which must succeed with `[SUCCESS]`. Final manual hardware verification (Task 5) confirms the visuals actually look right on the physical OLED, since that cannot be verified from a build alone.

---

### Task 1: Add `drawIconScaled()` and `drawArcUpperHalf()` primitives

**Files:**
- Modify: `firmware/src/display_manager.h`
- Modify: `firmware/src/display_manager.cpp`

- [ ] **Step 1: Add the two new function declarations to the header**

In `firmware/src/display_manager.h`, find this line (around line 114):

```cpp
void drawIcon(int x, int y, Icon icon);
void drawIcon24(int x, int y, Icon icon);   // for the 24x24 page icons
```

Replace it with:

```cpp
void drawIcon(int x, int y, Icon icon);
void drawIcon24(int x, int y, Icon icon);   // for the 24x24 page icons

// Nearest-neighbor scales the given 24x24 XBM icon and draws it centred at
// (cx, cy). `scale` must be in (0, 1] — 1.0 renders at the native 24x24 size,
// smaller values render progressively smaller (e.g. 0.6 renders ~14x14).
// Reuses the same 24x24 bitmaps as drawIcon24() — no separate small-icon
// assets needed.
void drawIconScaled(int cx, int cy, Icon icon, float scale);
```

Then find this line (around line 161):

```cpp
void drawCircle(int cx, int cy, int r);
void drawFilledCircle(int cx, int cy, int r);
void clearCircle(int cx, int cy, int r);   // bg-colour filled disc
```

Replace it with:

```cpp
void drawCircle(int cx, int cy, int r);
void drawFilledCircle(int cx, int cy, int r);
void clearCircle(int cx, int cy, int r);   // bg-colour filled disc

// Upper-half circle (arc opening upward) at radius r, centred at (cx, cy).
// Used for the WiFi-signal-search connecting animation's concentric arcs.
void drawArcUpperHalf(int cx, int cy, int r);
```

- [ ] **Step 2: Implement `drawIconScaled()` in display_manager.cpp**

In `firmware/src/display_manager.cpp`, find the existing `drawIcon24()` implementation:

```cpp
void drawIcon24(int x, int y, Icon icon) {
    g_u8g2.setDrawColor(1);
    g_u8g2.drawXBMP(x, y, 24, 24, icon24_xbm(icon));
}
```

Add the new function immediately after it:

```cpp
void drawIcon24(int x, int y, Icon icon) {
    g_u8g2.setDrawColor(1);
    g_u8g2.drawXBMP(x, y, 24, 24, icon24_xbm(icon));
}

void drawIconScaled(int cx, int cy, Icon icon, float scale) {
    if (scale <= 0.f) return;
    const uint8_t *src = icon24_xbm(icon);
    if (!src) return;
    const int SRC = 24;   // native icon width/height
    int dstSize = (int)(SRC * scale + 0.5f);
    if (dstSize < 1) return;
    int originX = cx - dstSize / 2;
    int originY = cy - dstSize / 2;
    g_u8g2.setDrawColor(1);
    for (int dy = 0; dy < dstSize; dy++) {
        int sy = (dy * SRC) / dstSize;
        for (int dx = 0; dx < dstSize; dx++) {
            int sx = (dx * SRC) / dstSize;
            int byteIdx = sy * 3 + (sx / 8);   // 3 bytes/row, LSB-first
            int bitIdx  = sx % 8;
            if (src[byteIdx] & (1 << bitIdx)) {
                g_u8g2.drawPixel(originX + dx, originY + dy);
            }
        }
    }
}
```

This must be placed after the `icon24_xbm()` function definition (around line 225 in the current file) so the name is visible — both are in the same translation unit already, so no header changes are needed for this internal call.

- [ ] **Step 3: Implement `drawArcUpperHalf()` in display_manager.cpp**

Find the existing circle primitives:

```cpp
void drawCircle(int cx, int cy, int r)     { g_u8g2.setDrawColor(1); g_u8g2.drawCircle(cx, cy, r); }
void drawFilledCircle(int cx, int cy, int r) {
    g_u8g2.setDrawColor(1); g_u8g2.drawDisc(cx, cy, r);
}
void clearCircle(int cx, int cy, int r) {
    g_u8g2.setDrawColor(0); g_u8g2.drawDisc(cx, cy, r); g_u8g2.setDrawColor(1);
}
```

Replace it with:

```cpp
void drawCircle(int cx, int cy, int r)     { g_u8g2.setDrawColor(1); g_u8g2.drawCircle(cx, cy, r); }
void drawFilledCircle(int cx, int cy, int r) {
    g_u8g2.setDrawColor(1); g_u8g2.drawDisc(cx, cy, r);
}
void clearCircle(int cx, int cy, int r) {
    g_u8g2.setDrawColor(0); g_u8g2.drawDisc(cx, cy, r); g_u8g2.setDrawColor(1);
}
void drawArcUpperHalf(int cx, int cy, int r) {
    g_u8g2.setDrawColor(1);
    g_u8g2.drawCircle(cx, cy, r, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
}
```

- [ ] **Step 4: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]` — these are additive functions with no call sites yet, so nothing else should change.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/display_manager.h firmware/src/display_manager.cpp
git commit -m "feat(firmware): add drawIconScaled and drawArcUpperHalf primitives

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Deep-sleep shutdown animation — moon & stars settle

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Replace the 3-phase text/CRT-bars sequence with moon & stars**

In `firmware/src/main.cpp`, find the `enterDeepSleep()` function's animation block (currently starts around line 2012). The existing code from the `if (oledFound) {` line through the matching `dm::hardClear();\n  }` looks like this:

```cpp
  if (oledFound) {
    // Sleep sequence, three visually-distinct phases:
    //   Phase 1 (0..600ms):  "GOOD NITE" reveal — text wipes in from top
    //   Phase 2 (600..1000ms): CRT-off vertical collapse — top+bottom black
    //                          bars converge to a bright midline
    //   Phase 3 (1000..1200ms): midline shrinks horizontally to a centre dot,
    //                          then a brief flash, then black.
    const int P1_FRAMES = 12;
    const int P2_FRAMES = 8;
    const int P3_FRAMES = 4;

    // Phase 1: reveal via retracting bg mask from bottom.
    for (int i = 0; i < P1_FRAMES; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "SLEEP", false, dm::ICON_WIFI);
      dm::setFont(dm::FONT_HUGE);
      const char *msg = "GOOD";
      int mw = dm::textWidth(msg);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw) / 2, OLED_OFFSET_Y + 12, msg);
      dm::setFont(dm::FONT_LARGE);
      const char *msg2 = "NITE";
      int mw2 = dm::textWidth(msg2);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw2) / 2, OLED_OFFSET_Y + 30, msg2);
      // Retract bg mask from bottom
      int revealH = (OLED_H - 10) * (P1_FRAMES - i) / P1_FRAMES;
      if (revealH > 0) {
        dm::clearRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - revealH,
                      OLED_W, revealH);
      }
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // Phase 2: CRT-off vertical collapse.
    for (int i = 0; i <= P2_FRAMES; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "SLEEP", false, dm::ICON_WIFI);
      dm::setFont(dm::FONT_HUGE);
      const char *msg = "GOOD";
      int mw = dm::textWidth(msg);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw) / 2, OLED_OFFSET_Y + 12, msg);
      dm::setFont(dm::FONT_LARGE);
      const char *msg2 = "NITE";
      int mw2 = dm::textWidth(msg2);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw2) / 2, OLED_OFFSET_Y + 30, msg2);

      int midY = OLED_OFFSET_Y + 10 + (OLED_H - 10) / 2;
      int barMax = (OLED_H - 10) / 2;
      int barH = (barMax * i) / P2_FRAMES;
      dm::clearRect(OLED_OFFSET_X, midY - barH, OLED_W, barH);
      dm::clearRect(OLED_OFFSET_X, midY, OLED_W, barH);
      dm::drawHLine(OLED_OFFSET_X, midY, OLED_W);
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // Phase 3: horizontal collapse of the midline to a centre dot.
    for (int i = 0; i <= P3_FRAMES; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      int midY = OLED_OFFSET_Y + 10 + (OLED_H - 10) / 2;
      int lineW = (OLED_W * (P3_FRAMES - i)) / P3_FRAMES;
      int lineX = OLED_OFFSET_X + (OLED_W - lineW) / 2;
      if (lineW > 0) dm::drawHLine(lineX, midY, lineW);
      if (i == P3_FRAMES) {
        dm::drawFilledCircle(OLED_OFFSET_X + OLED_W / 2, midY, 1);
      }
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    dm::hardClear();
  }
```

Replace the entire block above with:

```cpp
  if (oledFound) {
    // Sleep sequence, two visually-distinct phases:
    //   Phase 1 (0..~900ms): moon + 3 stars shown; stars blink out one at a
    //                        time, sequentially.
    //   Phase 2 (~900..1350ms): moon shrinks to a point, brief flash, black.
    const int STAR_FRAMES   = 6;  // ~300ms per star at 50ms/frame
    const int SHRINK_FRAMES = 8;  // ~400ms moon shrink at 50ms/frame

    const int moonX = OLED_OFFSET_X + OLED_W / 2;
    const int moonY = OLED_OFFSET_Y + 10 + (OLED_H - 10) / 2;
    const int moonRMax = 10;
    struct StarPos { int x, y; };
    const StarPos stars[3] = {
      {OLED_OFFSET_X + 14, OLED_OFFSET_Y + 14},
      {OLED_OFFSET_X + 46, OLED_OFFSET_Y + 12},
      {OLED_OFFSET_X + 50, OLED_OFFSET_Y + 24},
    };

    // Phase 1: stars blink out one at a time (moon stays full size).
    for (int i = 0; i < STAR_FRAMES * 3; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "SLEEP", false, dm::ICON_WIFI);
      // Crescent moon: filled circle with an offset black circle carved out.
      dm::drawFilledCircle(moonX, moonY, moonRMax);
      dm::clearCircle(moonX + moonRMax / 2, moonY - moonRMax / 3, (int)(moonRMax * 0.85f));
      for (int s = 0; s < 3; s++) {
        int outAtFrame = (s + 1) * STAR_FRAMES;
        if (i < outAtFrame) {
          dm::drawFilledRect(stars[s].x, stars[s].y, 2, 2);
        }
      }
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // Phase 2: moon shrinks to a point, brief flash, black.
    for (int i = 0; i <= SHRINK_FRAMES; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "SLEEP", false, dm::ICON_WIFI);
      int r = moonRMax * (SHRINK_FRAMES - i) / SHRINK_FRAMES;
      if (r > 0) {
        dm::drawFilledCircle(moonX, moonY, r);
        dm::clearCircle(moonX + r / 2, moonY - r / 3, (int)(r * 0.85f));
      }
      if (i == SHRINK_FRAMES) {
        dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, OLED_H);
      }
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    dm::hardClear();
  }
```

Everything after this block (LED pin detach, session/runtime bookkeeping, `esp_deep_sleep_start()`) is unchanged — do not touch anything below the closing `}` of this `if (oledFound)` block.

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): replace deep-sleep CRT-bars animation with moon & stars settle

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Menu cover-flow — scale-depth carousel

**Files:**
- Modify: `firmware/src/display_manager.cpp`

- [ ] **Step 1: Update `drawIconMenu()` to scale icons by distance from centre**

Find the current `drawIconMenu()` function:

```cpp
void drawIconMenu(const char *const *labels, const Icon *icons, int count,
                  float fractionalIdx) {
    if (count <= 0) return;
    int W = bufferWidth();
    int H = bufferHeight();
    int centreX = W / 2;
    int iconY = 12;          // below header
    int slot = 32;           // horizontal spacing between icons

    int base = (int)floorf(fractionalIdx);
    float frac = fractionalIdx - base;

    // Draw icons at positions relative to centre: -1, 0, +1, +2
    for (int rel = -1; rel <= 2; rel++) {
        int i = ((base + rel) % count + count) % count;
        int xCentre = centreX + (int)((rel - frac) * slot);
        int x = xCentre - 12;
        if (x + 24 < 0 || x >= W) continue;
        drawIcon24(x, iconY, icons[i]);
    }

    // Punch out the sides so only the centred icon is fully visible; the ones
    // to the left/right appear ghosted (partial visibility).
    // Left curtain: clear a 12-px column at x=0..7
    for (int y = iconY; y < iconY + 24; y++) {
        if (y >= H) break;
    }

    // Label under the centre icon: interpolates from current -> next.
```

Replace the section from the function signature through the (now-removed) punch-out loop with:

```cpp
void drawIconMenu(const char *const *labels, const Icon *icons, int count,
                  float fractionalIdx) {
    if (count <= 0) return;
    int W = bufferWidth();
    int centreX = W / 2;
    int iconY = 12;          // below header
    int slot = 32;           // horizontal spacing between icons

    int base = (int)floorf(fractionalIdx);
    float frac = fractionalIdx - base;

    // Draw icons at positions relative to centre: -1, 0, +1, +2. The centre
    // icon (rel == frac) renders at full size; neighbors shrink smoothly as
    // they move away from centre, giving a scale-depth carousel feel.
    for (int rel = -1; rel <= 2; rel++) {
        int i = ((base + rel) % count + count) % count;
        float dist = fabsf((float)rel - frac);
        float scale = 1.0f - fminf(1.0f, dist) * 0.4f;
        int xCentre = centreX + (int)((rel - frac) * slot);
        int yCentre = iconY + 12;
        if (xCentre + 12 < 0 || xCentre - 12 >= W) continue;
        drawIconScaled(xCentre, yCentre, icons[i], scale);
    }

    // Label under the centre icon: interpolates from current -> next.
```

Note: `int H = bufferHeight();` is intentionally removed — it was only read by the deleted punch-out loop. The rest of the function (the label-drawing code shown below this point) never references `H`, so removing it is safe and avoids an unused-variable warning.

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/display_manager.cpp
git commit -m "feat(firmware): scale-depth carousel for menu cover-flow

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: WiFi-connecting animation — signal-strength search

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Replace the radar-rings block with signal-arc cycling**

In `firmware/src/main.cpp`, inside `uiTask()`'s `SS_CONNECTING` branch, find:

```cpp
        } else if (currentState == SS_CONNECTING) {
          // Expanding wifi arcs pulsing outward from a centre dot.
          dm::drawFilledCircle(cx, cy, 2);
          int phase = (nowMs / 120) % 12;
          for (int i2 = 0; i2 < 3; i2++) {
            int r = ((phase + i2 * 4) % 12) + 3;
            if (r >= 4 && r <= 12) dm::drawCircle(cx, cy, r);
          }
          dm::setFont(dm::FONT_NORMAL);
          dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 20, uiLine1.c_str());
          dm::setFont(dm::FONT_SMALL);
          dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "[TAP] CANCEL");
```

Replace it with:

```cpp
        } else if (currentState == SS_CONNECTING) {
          // WiFi signal-strength search: dot always on, 3 arcs light up one
          // at a time bottom-to-top, then all drop out and the cycle repeats
          // — reuses the real WiFi glyph shape instead of generic radar rings.
          dm::drawFilledCircle(cx, cy, 2);
          int level = (nowMs / 280) % 5;   // 0..3 = arc count, 4 = dot-only pause
          const int radii[3] = {7, 12, 17};
          for (int i2 = 0; i2 < 3; i2++) {
            if (level > i2) dm::drawArcUpperHalf(cx, cy, radii[i2]);
          }
          dm::setFont(dm::FONT_NORMAL);
          dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 20, uiLine1.c_str());
          dm::setFont(dm::FONT_SMALL);
          dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "[TAP] CANCEL");
```

- [ ] **Step 2: Build to verify**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): replace WiFi-connecting radar rings with signal-search arcs

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Manual hardware verification

**Files:** none (verification only)

- [ ] **Step 1: Flash the firmware**

Run: `cd firmware; & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload`

- [ ] **Step 2: Verify deep-sleep animation**

Trigger sleep (via menu → Deep Sleep → confirm, or button-hold path per existing UX). Confirm:
- "SLEEP" header shows with a crescent moon and 3 stars.
- Stars blink out one at a time, not all at once.
- After all stars are out, the moon shrinks smoothly to a point.
- A brief flash occurs right before the screen goes black.
- Total sequence feels roughly as long as before (~1.3s), not noticeably longer or cut short.

- [ ] **Step 3: Verify menu scale-depth carousel**

Navigate the main menu (short button presses to move between items). Confirm:
- The centre (selected) icon renders visibly larger than its neighbors.
- Neighbor icons shrink smoothly as they move away from centre during the scroll animation (not a sudden pop).
- No visual artifacts (missing pixels, icon shape looking broken) at any scale level — check a few different icons, not just one.
- Label text under the centre icon still reads correctly and stays in sync with the icon position.

- [ ] **Step 4: Verify WiFi-connecting animation**

Trigger a WiFi connect (e.g. Measure or Weather from a cold state requiring a fresh connection). Confirm:
- A dot is always visible at the connecting icon's centre.
- The 3 signal arcs light up one at a time, bottom-to-top (smallest arc first).
- After all 3 are lit, they all drop out together and the cycle restarts from the dot.
- The animation reads clearly as a "WiFi searching for signal" motif, distinct from the orbiting-dot (SYNCING) animation and the rotating sweep-line (SCANNING/LOCATING) animations.

- [ ] **Step 5: Confirm no regressions to untouched screens**

Quickly check that SYNCING (orbiting dot), SCANNING/LOCATING (sweep line), and the toast banner still look and behave exactly as before — none of these were touched by this plan, so this step should be a formality, but confirms no accidental cross-contamination from the shared `uiTask()` function edits in Task 4.

This step requires physical device access and cannot be automated — mark it done only after actually observing the behavior on hardware.
