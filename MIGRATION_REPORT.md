# AETHER_OS Display Migration Report — Adafruit_GFX/SSD1306 → U8g2

## Memory footprint

| Stage                                    | Flash (bytes) | Flash % | RAM (bytes) | RAM %  |
|------------------------------------------|--------------:|--------:|------------:|-------:|
| **Baseline** (Adafruit_GFX + SSD1306)    |     1,067,533 |  81.4%  |      49,944 | 15.2%  |
| Stage A — dm:: abstraction (Adafruit backend) |    (skipped, user opted for combined A+B) |    |    |    |
| **Stage B** — U8g2 backend, viewport unchanged |   1,071,349 |  81.7%  |      51,040 | 15.6%  |
| **Stage C** — Full 128×64 attempt (rolled back)  | 1,072,349 |  81.8%  |      51,088 | 15.6%  |
| **64×48 native** — U8g2 ER constructor, layout fixed | — | — | — | — |
| **Motion pass** — Tween/toast/splash/cover-flow menu | 1,075,873 | 82.1% | 50,520 | 15.4% |
| **Page redesigns** — Clock/Weather/Stats/Locate/Measure/Sleep bespoke layouts | 1,077,745 | 82.2% | 50,544 | 15.4% |
| **Final polish** — error screens, locked states, per-state progress animations, toast confirmations, saved-WiFi viewer | 1,078,009 | 82.2% | 50,544 | 15.4% |

**Total delta vs baseline:** +8,340 B Flash (+0.7 pp), **+576 B RAM (+0.2 pp)**.

- RAM is now *below* the "Stage C" attempt because switching to the 64×48 ER constructor cut the framebuffer from 1024 B → 384 B (saved 640 B).
- Flash added ~4 KB for the motion layer: `dm::Tween`, `Icon24` × 14 XBMs (~1 KB PROGMEM), toast state, boot splash routine, cover-flow menu, `blitShifted`.
- Adafruit_GFX + Adafruit_SSD1306 no longer appear in the dependency graph (verified).

Build command: `pio run` on `env:esp32dev` (arduino / esp32dev).

## Motion / UI polish pass (Phases 1–4)

Landed after the initial U8g2 migration:

- **`dm::Tween`** — millis-based ease-out cubic float interpolator.
- **Menu scroll animation** — press advances `currentMenuIndex` and kicks a 180 ms tween; `uiTask` polls tween value each frame and re-renders the menu at the interpolated position. Shortest-signed-delta wrap-around (9→0 scrolls forward, not through all rows).
- **Cover-flow icon menu** — replaces the text-list menu with a centred 24×24 icon + label, prev/next icons visible at edges. 14 XBM icons authored (10 main pages + 4 wifi sub-menu items). Same tween drives horizontal scroll.
- **Boot splash** — 900 ms cold-boot sequence: expanding centre dot → `AETHER` sweep-reveal → `v2.0` dither fade-in. Skipped on button-wake to preserve fast wake.
- **Non-blocking toasts** — `dm::toast(text, ms)` slides a 12 px banner from y=−12, holds, retracts. Used for LED / SLEEP / CLOUD OK confirmations. Replaces the 1.5–2 s `updateOLED` freezes that used to block the menu.
- **State-reset audit** — fixed lockups in `showWeatherPage`, `showTimePage`, `runLocatePage`, `runMeasurementFlow`: every early return path now restores `currentState = SS_MENU` so uiTask and the button handler don't get stuck on `SS_SYNCING`/`SS_CONNECTING`.
- **Adaptive frame budget** — uiTask polls at 16 ms (60 fps) while any tween is active, drops to 50 ms otherwise. On 64×48 @ 400 kHz I2C, a `sendBuffer()` is ~4 ms, so 60 fps costs ~24% of one core.
- **I2C mutex** — display mutex now also guards MPU6050 reads (originally added in Stage B, mentioned here for completeness).

## sendBuffer() timing

384-byte framebuffer @ 400 kHz I2C ≈ 8 ms per commit (worst-case). `uiTask` runs at 16 ms during animations (60 fps target) and 50 ms otherwise. Even at 60 fps, wire time is ~50% of the frame; the remainder is available for U8g2 drawing calls, which are cheap in software rasterization.

## Suggestions for future UI improvements

- **Per-screen dirty hash** — hash of the framebuffer at `endFrame()`; skip `sendBuffer()` on repeats. Would cut I2C traffic on the menu when no tween is active.
- **`logisoso28` clock** — the numeric-only font is loaded but not yet used by `showTimePage`; wiring HH:MM through a dedicated `dm::drawClock()` is a one-line win.
- **Odometer value-roll** — planned in Phase 3 but not yet wired into `runMeasurementFlow`. During each 2.5 s sample, tween the temp/hum/LDR readouts between old and new value for an odometer feel.
- **Page enter/exit slide** — the `blitShifted` primitive is in place (`slidePush` architecture) but not yet invoked at menu ↔ subpage transitions. Would take a saved-buffer snapshot before entering a subpage and animate a 220 ms horizontal slide.
- **Sensor sparklines** — with the icon menu removing text density, a small trend line under sensor values is essentially free.
- **Cover-flow polish** — the current cover-flow renders neighbouring icons at fixed 32 px slot spacing; add scale / dim by distance from centre for extra depth.
