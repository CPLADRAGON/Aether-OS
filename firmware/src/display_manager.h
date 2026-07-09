// display_manager.h — thin abstraction over U8g2 for AETHER_OS.
// Only display_manager.cpp includes U8g2 headers directly; the rest of the
// firmware talks to the display through this API.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace dm {

enum Font : uint8_t {
    FONT_SMALL,   // ~5x7, header / footnotes
    FONT_NORMAL,  // ~6x10, body / menus (matches Adafruit default cell)
    FONT_LARGE,   // ~10x20, values / spinner (matches Adafruit size 2)
    FONT_HUGE     // ~19x28, clock digits (numeric only)
};

enum Icon : uint8_t {
    ICON_WIFI = 0,
    ICON_CLOUD,
    ICON_PIN,
    ICON_SCAN,
    // 24x24 page icons for cover-flow menu (Phase 4)
    ICON_MEASURE_LG,
    ICON_TIME_LG,
    ICON_WEATHER_LG,
    ICON_WEATHER_SUN_LG,
    ICON_WEATHER_RAIN_LG,
    ICON_WEATHER_STORM_LG,
    ICON_WEATHER_SNOW_LG,
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

// Non-blocking eased float interpolator. millis()-based; call value() to poll
// the current value and observe active() to know if it's still moving.
struct Tween {
    float    start = 0.f;
    float    end   = 0.f;
    uint32_t t0Ms  = 0;
    uint16_t durMs = 0;
    bool     running = false;

    void start_(float from, float to, uint16_t durationMs);
    float value();         // current interpolated value; clears running when finished
    bool  active() const { return running; }
    bool  completed(uint32_t nowMs) const { return running && (nowMs - t0Ms) >= durMs; }
};

float easeOutCubic(float x);        // x in [0,1]
float easeInOutCubic(float x);      // x in [0,1]

// Non-blocking, millis()-based animation cell. Each Animation owns its own
// interval + frame counter; tick() returns true when the frame advanced.
// Callers should treat a true return as a dirty hint (markDirty()).
struct Animation {
    uint8_t  frame;
    uint32_t lastUpdateMs;
    uint16_t intervalMs;

    Animation() : frame(0), lastUpdateMs(0), intervalMs(120) {}
    explicit Animation(uint16_t iv) : frame(0), lastUpdateMs(0), intervalMs(iv) {}

    bool tick();
    void reset() { frame = 0; lastUpdateMs = 0; }
};

// --- Lifecycle -------------------------------------------------------------
// Probes I2C for 0x3C then 0x3D, calls u8g2.begin(), configures baseline.
// Requires Wire.begin() to have been called by the caller.
// Returns true if a display was found and initialised.
bool init(SemaphoreHandle_t sharedI2cMutex);

// True once init() has succeeded.
bool ready();

// Force a full clear + commit (used at boot and on power-down).
void hardClear();

// --- Frame lifecycle -------------------------------------------------------
// beginFrame() takes the I2C mutex (with timeout) and clears the framebuffer.
// Returns false if the mutex could not be acquired — caller should NOT draw.
bool beginFrame(uint32_t timeoutMs = 100);

// endFrame() commits the buffer to the panel iff dirty, then releases the
// mutex. Safe to call unconditionally after a successful beginFrame().
void endFrame();

// Mark the current frame dirty. beginFrame() implicitly marks dirty for
// backwards compatibility with the "full redraw every tick" model; call
// this explicitly only when using the dirty-check fast path.
void markDirty();

// --- Text / primitives -----------------------------------------------------
void setFont(Font f);
int  textWidth(const char *s);
int  fontAscent();  // top-to-baseline; useful for vertical centring

void drawText(int x, int y, const char *s);
// Renders `s` in the current foreground colour; caller controls colour via
// drawFilledRect() beforehand for inverted rows.
void drawTextInverted(int x, int y, const char *s);

void drawIcon(int x, int y, Icon icon);
void drawIcon24(int x, int y, Icon icon);   // for the 24x24 page icons

// Nearest-neighbor scales the given 24x24 XBM icon and draws it centred at
// (cx, cy). `scale` must be > 0 — 1.0 renders at the native 24x24 size,
// smaller values render progressively smaller (e.g. 0.6 renders ~14x14).
// No upper bound is enforced, but callers in this codebase only ever pass
// values <= 1.0 (shrinking, never upscaling beyond the native bitmap).
// Reuses the same 24x24 bitmaps as drawIcon24() — no separate small-icon
// assets needed.
void drawIconScaled(int cx, int cy, Icon icon, float scale);

// Direct framebuffer access for slide transitions. Returns the internal U8g2
// buffer (384 B on 64x48) and its length. Caller must have taken the frame lock.
uint8_t *rawBuffer();
size_t   rawBufferLen();
int      bufferWidth();
int      bufferHeight();

// Copy `src` (same dimensions as internal buffer) shifted by dxPixels into the
// internal buffer. Wraps by clearing off-panel columns.
void blitShifted(const uint8_t *src, int dxPixels);

// Cover-flow / animated menu: draws all N items scrolled to fractional index.
// `menuY` is the top of the row area; `rowH` is the row height in pixels.
// The middle row is highlighted with an XOR band.
void drawMenuAnimated(int menuY, int rowH, int visibleRows,
                      const char *const *items, int itemCount,
                      float fractionalIdx);

// Icon cover-flow variant (Phase 4). Draws current icon at centre, ghost
// prev/next icons at edges, label under centre.
void drawIconMenu(const char *const *labels, const Icon *icons, int count,
                  float fractionalIdx);

// Slide/toast/splash --------------------------------------------------------
enum SlideDir : uint8_t { SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN };

// Toast: 12 px banner slides down from top, holds, retracts. Non-blocking.
// Enqueued if another toast is currently on screen.
void toast(const char *text, uint16_t holdMs = 1200);
// Called by uiTask every frame; overlays the current toast if active. Returns
// true if a toast is currently visible (caller may wish to force dirty).
bool toastTick();

// Boot splash: blocks for ~900 ms, plays expanding dot + AETHER sweep + subtitle
// fade-in. Safe to call before init() has finished. Skipped if !ready().
void bootSplash();

void drawPixel(int x, int y);
void drawHLine(int x, int y, int w);
void drawVLine(int x, int y, int h);
void drawLine(int x0, int y0, int x1, int y1);
void drawRect(int x, int y, int w, int h);
void drawFilledRect(int x, int y, int w, int h);
void clearRect(int x, int y, int w, int h);  // fills with background
void drawCircle(int cx, int cy, int r);
void drawFilledCircle(int cx, int cy, int r);
void clearCircle(int cx, int cy, int r);   // bg-colour filled disc

// Upper-half circle (arc opening upward) at radius r, centred at (cx, cy).
// Used for the WiFi-signal-search connecting animation's concentric arcs.
void drawArcUpperHalf(int cx, int cy, int r);

// --- Composed helpers ------------------------------------------------------
// Header bar: inverted 12px strip at y=0..11 with left-aligned title and
// optional 8px icon at the right edge. Coordinates respect OLED_OFFSET_*.
void drawHeader(int originX, int originY, int width,
                const char *title, bool showIcon, Icon icon);

// Menu row: draws a 10-11px row at (originX, y) with optional inverted
// background when `selected` is true.
void drawMenuRow(int originX, int y, int width, int rowHeight,
                 const char *text, bool selected);

// Progress bar outline + fill.
void drawProgressBar(int x, int y, int w, int h, uint8_t percent);

// One-shot status screen mirroring the legacy updateOLED signature. Wraps
// beginFrame()/endFrame() internally. Non-blocking mutex acquire (portMAX_DELAY
// in the original code path).
void showStatus(int originX, int originY, int width,
                const char *header, const char *line1, const char *line2,
                const char *line3, bool showIcon, Icon icon);

}  // namespace dm
