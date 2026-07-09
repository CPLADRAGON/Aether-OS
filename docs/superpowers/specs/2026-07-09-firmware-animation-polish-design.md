# Firmware Animation Polish — Design Spec

**Date:** 2026-07-09
**Status:** Approved by user, ready for implementation planning

## Context

AETHER_OS firmware (ESP32, U8g2-driven 64x48 monochrome OLED) has a mix of animation
quality: the menu cover-flow scroll and toast banner already use a proper non-blocking
eased `dm::Tween` (ease-out-cubic). Three other animations were identified as weaker —
either using raw linear frame-stepping with no easing, or a visual concept that reads as
mechanical/busy. This spec covers a full concept-level redesign of those three, decided
collaboratively via the visual companion (browser mockups simulated at true 64x48 scale).

**Explicitly out of scope:** the toast banner (`dm::toastTick()`) — already uses
`easeOutCubic` for slide in/out, confirmed during investigation to be fine as-is. No
changes to toast in this pass.

## 1. Deep-sleep shutdown animation

**File:** `firmware/src/main.cpp`, function `enterDeepSleep()` (currently ~line 2012)

**Current behavior:** 3 blocking phases, pure linear interpolation, no easing:
1. "GOOD NITE" text reveals via a retracting black mask (12 frames, 50ms each)
2. CRT-off vertical collapse — black bars converge to a bright midline (8 frames)
3. Midline shrinks horizontally to a centre dot, brief flash, black (4 frames)

**New behavior — "Moon & stars settle":**
1. Draw "SLEEP" header (reuse existing `dm::drawHeader()` call, unchanged).
2. Draw a crescent moon (large filled circle via `dm::drawFilledCircle()`, then an
   offset black circle drawn on top to carve the crescent shape — same technique
   validated in the browser mockup) at a fixed position, plus 3 small stars (each a
   2x2 filled square via `dm::drawFilledRect()`) at fixed positions around the moon.
3. Stars blink out one at a time, sequentially, over ~1400ms total (about 470ms per
   star) — each star simply stops being drawn once its "out" time is reached, keeping
   the linear/stepped timing style consistent with the rest of the codebase (per user's
   choice to keep linear timing, only the concept changed).
4. After all stars are out, the moon shrinks from its full radius to 0 over ~500ms
   (linear interpolation, matching the existing code's style: `r = rMax * (1 - i/N)`).
5. One-frame full-screen flash (invert draw color briefly), then `dm::hardClear()`.
6. Total duration stays approximately the same as the current sequence (~1.1–1.3s) —
   no change to the sleep-entry timing budget elsewhere in the codebase.
7. Everything after the animation (LED pin detach, session/runtime bookkeeping,
   `esp_deep_sleep_start()`) is unchanged.

This is a one-shot, blocking sequence (same as today) — it runs once immediately before
deep sleep, so blocking is acceptable and matches the existing implementation's
structure. No new state machine or non-blocking animation object is needed here.

## 2. Menu cover-flow → scale-depth carousel

**Files:**
- `firmware/src/display_manager.h` / `.cpp` — `drawIconMenu()` (currently ~line 531)

**Current behavior:** All icons drawn at fixed 24x24 via `drawIcon24()` /
`g_u8g2.drawXBMP()`, sliding horizontally only; centre one highlighted by position,
neighbors partially clipped at the screen edges (the "ghosting" clip code mentioned in
a comment is actually a no-op today — dead code, to be removed as part of this change
since we're touching this function anyway).

**New behavior:** Centre icon renders larger, neighbors render smaller, continuously
interpolated by distance from centre (matches the approved "scale-depth carousel"
mockup).

**Implementation approach:**
- U8g2's `drawXBMP()` only draws bitmaps at native 1:1 pixel size — no built-in
  scaling. Add a new primitive to `display_manager`:
  ```cpp
  void drawIconScaled(int cx, int cy, Icon icon, float scale); // scale in (0, 1]
  ```
  This reads the existing 24x24 XBM source bit-by-bit (same bit-layout helper logic
  as the existing XBM path) and writes each source pixel to a nearest-neighbor-mapped
  destination position/size via `dm::drawPixel()`, centered on `(cx, cy)`. No new
  icon assets are needed — this reuses the existing 24x24 bitmaps at any requested
  size.
- `drawIconMenu()` computes each visible icon's scale from its distance from the
  (fractional) centre index: `scale = 1.0 - min(1.0, abs(rel - frac)) * 0.4` — a
  neighbor at full distance (rel-frac = ±1) renders at 60% size; the centre icon
  (rel-frac = 0) renders at 100%. This uses the same `frac` value already computed
  from the existing `menuTween`, so no changes to the tween/timing system itself —
  only how the result is rendered. (0.4 is a starting point tuned to look right in
  the browser mockup's proportions; adjust during implementation if it looks off at
  actual panel resolution.)
- Remove the dead "punch out the sides" no-op loop currently in `drawIconMenu()`.
- CPU cost: nearest-neighbor sampling a 24x24 source is at most 576 pixel writes per
  icon, x3 visible icons per frame ≈ 1728 `drawPixel()` calls/frame — trivial for the
  ESP32 at the existing ~150ms UI tick rate.

**Fallback if the scaler proves too slow/complex during implementation:** pre-author
one smaller fixed-size icon set (discussed but not the primary approach) and snap
discretely between big/small rather than smoothly scaling. This spec's primary
direction is the smooth scaler; the plan should implement that first and only fall
back if a concrete performance problem is measured.

## 3. WiFi-connecting animation → signal search

**File:** `firmware/src/main.cpp`, `uiTask()`'s `SS_CONNECTING` branch (currently
~line 619), plus one new primitive in `display_manager`.

**Current behavior:** 3 concentric full circles pulse outward simultaneously from a
centre dot, stepped every 120ms (`phase = (nowMs/120) % 12`), no easing — reads as
busy/mechanical, and doesn't especially read as "WiFi" (just generic radar rings).

**New behavior — signal-strength cycling:** dot always drawn; 3 upward-opening arcs
(representing signal strength levels, like a phone's WiFi icon) light up one at a
time bottom-to-top, then all drop out and the cycle restarts. This directly reuses
the real WiFi glyph shape rather than a generic radar-ring motif, and is visually
distinct from the orbiting-dot (SYNCING) and rotating-sweep-line (SCANNING/LOCATING)
motifs already used on neighboring states.

**Implementation approach:**
- U8g2 natively supports drawing partial circles via a quadrant option mask
  (`U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT` = upper half). This produces exactly
  the upward-opening arc/fan shape needed — no manual per-degree angle-stepping loop
  required.
- Add to `display_manager`:
  ```cpp
  void drawArcUpperHalf(int cx, int cy, int r);
  ```
  wrapping `g_u8g2.drawCircle(cx, cy, r, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT)`.
- In the `SS_CONNECTING` branch: draw the centre dot unconditionally, then draw 0–3
  arcs (radii roughly 7/12/17px, tuned to fit the 64x48 panel) depending on the
  current level. Level advances every ~280ms (`level = (nowMs / 280) % 5`, treating
  the 5th step as the "all off, dot only" pause before the cycle repeats), following
  the same `nowMs`-based stepping style already used by the other connecting-state
  animations in this function (SYNCING's orbiting dot, SCANNING's sweep line) — no
  new animation framework needed, consistent with the surrounding code.

## Testing / verification

No test runner exists for firmware (per project conventions) — verification is
`pio run` (build must succeed) plus manual visual confirmation on hardware. Given
`enterDeepSleep()` and the `SS_CONNECTING` branch require real WiFi-connect/deep-sleep
event triggers to observe, and the menu carousel requires physical button navigation,
final visual confirmation of all three remains a manual hardware verification step
(consistent with the existing outstanding hardware-verification item from earlier
firmware work this session).

## Non-goals

- No changes to the toast banner animation (already good).
- No changes to the SYNCING orbiting-dot or SCANNING/LOCATING sweep-line animations —
  only WiFi-connecting's radar rings are being replaced.
- No new animation framework/abstraction beyond the one small `drawIconScaled()` and
  `drawArcUpperHalf()` primitives — everything else reuses existing patterns
  (`dm::Tween`, `nowMs`-based stepping, blocking one-shot sequences) already
  established in the codebase.
