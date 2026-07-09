// display_manager.cpp — U8g2 backend for AETHER_OS.
#include "display_manager.h"

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>
#include <string.h>

namespace {

// Full framebuffer, hardware I2C. 384 B for 64x48. The ER variant handles the
// physical column offset of the Wemos-style 64x48 SSD1306 panel — writing at
// x=0 lands at the visible left edge instead of falling off the panel.
U8G2_SSD1306_64X48_ER_F_HW_I2C g_u8g2(U8G2_R0, U8X8_PIN_NONE);

SemaphoreHandle_t g_i2cMutex = nullptr;
bool              g_ready    = false;
bool              g_dirty    = false;
bool              g_frameActive = false;
bool              g_inverted = false;  // current text colour (0=inv, 1=normal)

// ---- 12x12 XBM icons ------------------------------------------------------
// XBM = row-major, LSB-first within each byte. Each icon is 12 wide, so each
// row occupies 2 bytes (16 bits, top 4 bits of second byte are padding). These
// are re-authored (not the legacy 8x8 Adafruit bitmaps) for the full-panel
// redesign; Stage A/B still use them at 12x12 in the header.
//
// 8x8 header WiFi glyph — arcs radiating from a bottom dot, sized to fit
// cleanly inside the 10-px inverted header bar without clipping.
static const uint8_t xbm_wifi_hdr8[] PROGMEM = {
    0x3C, 0x42, 0x99, 0x24, 0x18, 0x00, 0x18, 0x00,
};

// wifi: three arcs + centre dot (12x12, used by cover-flow if needed)
static const uint8_t xbm_wifi[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0xf8, 0x01, 0x06, 0x06, 0x01, 0x08, 0xf0, 0x00, 0x08, 0x03, 0x00, 0x00, 
	0x60, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// cloud: rounded cloud silhouette with two internal drops
static const uint8_t xbm_cloud[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x01, 0x60, 0x06, 0x10, 0x04, 0x98, 0x0c, 0xc6, 0x19, 
	0x82, 0x20, 0x82, 0x20, 0x84, 0x20, 0xb8, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// pin: teardrop with hole
static const uint8_t xbm_pin[] PROGMEM = {
    0xE0, 0x00, 0xF0, 0x01, 0x38, 0x03, 0x18, 0x03,
    0x38, 0x03, 0xF0, 0x01, 0xE0, 0x00, 0x40, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// scan: crosshair square
static const uint8_t xbm_scan[] PROGMEM = {
    0xF0, 0x00, 0x08, 0x01, 0x24, 0x02, 0xA2, 0x04,
    0xF3, 0x0C, 0xAA, 0x05, 0xAA, 0x05, 0xF3, 0x0C,
    0xA2, 0x04, 0x24, 0x02, 0x08, 0x01, 0xF0, 0x00,
};

static const uint8_t *icon_xbm(dm::Icon i) {
    switch (i) {
        case dm::ICON_WIFI:  return xbm_wifi;
        case dm::ICON_CLOUD: return xbm_cloud;
        case dm::ICON_PIN:   return xbm_pin;
        case dm::ICON_SCAN:  return xbm_scan;
        default:             return xbm_wifi;
    }
}

// ---- 24x24 XBM page icons (Phase 4 cover-flow) ---------------------------
// XBM row major, LSB-first. 24 wide = 3 bytes/row * 24 rows = 72 bytes each.
// Kept intentionally geometric and readable at small physical size.

// measure: centred thermometer (tube at cols 11-12, bulb rows 18-22)
static const uint8_t xbm_measure_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x3c, 0x00, 
    0x00, 0x66, 0x00, 0x00, 0x66, 0x00, 0x00, 0x76, 0x00, 0x00, 0x66, 0x00, 
    0x00, 0x66, 0x00, 0x00, 0x76, 0x00, 0x00, 0x66, 0x00, 0x00, 0x7e, 0x00, 
    0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0xff, 0x00, 0x80, 0xff, 0x01, 
	0x80, 0xff, 0x01, 0x80, 0xff, 0x01, 0x80, 0xff, 0x01, 0x00, 0xff, 0x00, 
    0x00, 0x7e, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// time: clock face ring with hour/minute hands.
static const uint8_t xbm_time_lg[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x7e,0x00, 0xc0,0xff,0x03, 0xe0,0xc3,0x07,
    0x70,0x00,0x0e, 0x38,0x18,0x1c, 0x18,0x18,0x18, 0x0c,0x18,0x30,
    0x0c,0x18,0x30, 0x06,0x18,0x60, 0x06,0x18,0x60, 0x06,0x18,0x60,
    0x06,0x78,0x60, 0x06,0xe0,0x61, 0x06,0x80,0x61, 0x0c,0x00,0x30,
    0x1c,0x00,0x38, 0x18,0x00,0x18, 0x38,0x00,0x1c, 0x70,0x00,0x0e,
    0xe0,0xc3,0x07, 0xc0,0xff,0x03, 0x00,0x7e,0x00, 0x00,0x00,0x00,
};

// led: filled lightbulb with rays.
static const uint8_t xbm_led_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0xff, 0x00, 0xc0, 0xc3, 0x03, 0xc0, 
	0x00, 0x03, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x24, 0x06, 0x60, 0x18, 
	0x06, 0x60, 0x18, 0x06, 0xc0, 0x18, 0x03, 0x80, 0x99, 0x01, 0x80, 0xff, 0x01, 0x00, 0xff, 0x00, 
	0x00, 0xc3, 0x00, 0x00, 0xff, 0x00, 0x00, 0xc3, 0x00, 0x00, 0xff, 0x00, 0x00, 0x7e, 0x00, 0x00, 
	0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// interval: hourglass with sand
static const uint8_t xbm_interval_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x03, 0xc0, 0x00, 0x03, 0xf0, 0xff, 0x0f, 0xf8, 
	0xff, 0x1f, 0x18, 0x00, 0x18, 0x18, 0x00, 0x18, 0xf8, 0xff, 0x1f, 0xf8, 0xff, 0x1f, 0x18, 0x00, 
	0x18, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x0f, 0x18, 0x80, 0x1f, 0x18, 0xc0, 0x39, 
	0x18, 0xe0, 0x79, 0x18, 0xe0, 0x79, 0x18, 0xe0, 0x7b, 0x18, 0xe0, 0x77, 0xf8, 0xc7, 0x3f, 0xf0, 
	0x8f, 0x1f, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
};

// reset: circular arrow (open circle with arrowhead at top)
static const uint8_t xbm_reset_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x80, 0xff, 0x01, 0xc0, 
	0x81, 0x03, 0xe0, 0x00, 0x07, 0x60, 0x00, 0x0e, 0x30, 0x00, 0x0c, 0x10, 0x00, 0x08, 0x18, 0x00, 
	0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x07, 0x18, 0x80, 0x03, 0x10, 0xc0, 0x11, 0x30, 0xc3, 0x19, 
	0x70, 0xc3, 0x1f, 0xe0, 0xc3, 0x0f, 0xc0, 0xe3, 0x07, 0xf0, 0xe3, 0x00, 0xf0, 0x43, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// sleep: crescent moon + stars
static const uint8_t xbm_sleep_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x80, 0xc3, 0x00, 0xc0, 0xe3, 0x01, 0x70, 
	0xe3, 0x01, 0x30, 0xc2, 0x00, 0x18, 0x06, 0x00, 0x18, 0x06, 0x18, 0x0c, 0x0c, 0x18, 0x0c, 0x1c, 
	0x00, 0x0c, 0x38, 0x00, 0x0c, 0x70, 0x00, 0x0c, 0xe0, 0x01, 0x08, 0x80, 0x3f, 0x18, 0x00, 0x1e, 
	0x38, 0x00, 0x18, 0x30, 0x00, 0x0c, 0x60, 0x00, 0x06, 0xc0, 0x81, 0x07, 0x80, 0xff, 0x01, 0x00, 
	0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

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
static const uint8_t xbm_locate_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0xff, 0x00, 0xc0, 0xc3, 0x03, 0xc0, 
	0x00, 0x03, 0x60, 0x00, 0x06, 0x70, 0x00, 0x0e, 0x30, 0x18, 0x0c, 0x30, 0x3c, 0x0c, 0x30, 0x3c, 
	0x0c, 0x30, 0x18, 0x0c, 0x20, 0x00, 0x04, 0x60, 0x00, 0x06, 0xe0, 0x00, 0x07, 0xc0, 0x00, 0x03, 
	0x80, 0x81, 0x01, 0x80, 0xc3, 0x01, 0x00, 0xe7, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x18, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// stats: bar chart
static const uint8_t xbm_stats_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 
	0xc3, 0x00, 0x00, 0xc3, 0x00, 0x00, 0xc3, 0x00, 0x00, 0xc3, 0x00, 0x00, 0xc3, 0x3f, 0x00, 0xc3, 
	0x3f, 0xfc, 0xc3, 0x30, 0xfc, 0xc3, 0x30, 0x0c, 0xc3, 0x30, 0x0c, 0xc3, 0x30, 0x0c, 0xc3, 0x30, 
	0x0c, 0xc3, 0x30, 0x0c, 0xc3, 0x30, 0x0c, 0xc3, 0x30, 0xfc, 0xff, 0x3f, 0xfc, 0xff, 0x3f, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// wifi menu: three arcs (large)
static const uint8_t xbm_wifimenu_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xe0, 
	0xff, 0x07, 0xf8, 0xff, 0x0f, 0xfc, 0x00, 0x3f, 0x3e, 0x00, 0x7c, 0x0e, 0x00, 0x70, 0x04, 0x7c, 
	0x20, 0x80, 0xff, 0x01, 0xe0, 0xff, 0x07, 0xe0, 0xc3, 0x07, 0xc0, 0x00, 0x06, 0x00, 0x00, 0x00, 
	0x00, 0x18, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x18, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// portal (wifi cog)
static const uint8_t xbm_portal_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x03, 0x00, 0xf0, 0x0f, 0x00, 0x38, 0x1c, 0x00, 0x00, 
	0x00, 0x00, 0xe0, 0x07, 0x00, 0xc0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 
	0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0xf0, 0xff, 0x0f, 0xf8, 0xff, 0x1f, 0x18, 0x00, 0x18, 
	0x18, 0x26, 0x1b, 0x18, 0x26, 0x1b, 0x18, 0x00, 0x18, 0xf8, 0xff, 0x1f, 0xf0, 0xff, 0x0f, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// clear (trash can)
static const uint8_t xbm_clear_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0xf0, 0xff, 0x0f, 0xf0, 
	0xff, 0x0f, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x66, 0x06, 0x60, 0x66, 0x06, 0x60, 0x66, 
	0x06, 0x60, 0x66, 0x06, 0x60, 0x66, 0x06, 0x60, 0x66, 0x06, 0x60, 0x66, 0x06, 0x60, 0x66, 0x06, 
	0x60, 0x66, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0xe0, 0xff, 0x07, 0xc0, 0xff, 0x03, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// select (checkmark)
static const uint8_t xbm_select_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x66, 0x00, 0x30, 
	0xe7, 0x0e, 0xf0, 0xe7, 0x0f, 0xd8, 0x81, 0x1b, 0x18, 0x00, 0x18, 0x1c, 0x3c, 0x38, 0x70, 0x7e, 
	0x1e, 0x20, 0x7e, 0x04, 0x20, 0x7e, 0x04, 0x70, 0x7e, 0x0e, 0x1c, 0x3c, 0x38, 0x18, 0x00, 0x18, 
	0xd8, 0x81, 0x1b, 0xf0, 0xe7, 0x0f, 0x70, 0xe7, 0x0e, 0x00, 0x66, 0x00, 0x00, 0x7e, 0x00, 0x00, 
	0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// back (arrow left)
static const uint8_t xbm_back_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x1f, 0x00, 
	0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x80, 0x01, 0x18, 0xc0, 0x00, 0x18, 0x60, 0x00, 
	0x18, 0xf0, 0x7f, 0x18, 0xf0, 0x7f, 0x18, 0x60, 0x00, 0x18, 0xc0, 0x00, 0x18, 0x80, 0x01, 0x18, 
	0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0xf0, 0x1f, 0x00, 0xf0, 0x0f, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// room: house with peaked roof + outline walls + door
static const uint8_t xbm_room_lg[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x7e, 0x00, 0x00, 
	0xc7, 0x00, 0xc0, 0x81, 0x03, 0xe0, 0x00, 0x07, 0x70, 0x00, 0x0e, 0x1c, 0x00, 0x3c, 0x0e, 0x00, 
	0x7c, 0x04, 0x00, 0x2c, 0xb8, 0x03, 0x0c, 0xfc, 0x07, 0x0c, 0x46, 0x0c, 0x0c, 0x06, 0x0c, 0x0c, 
	0x46, 0x0c, 0x0c, 0xec, 0x06, 0x0c, 0x46, 0xcc, 0x0f, 0x06, 0xcc, 0x0f, 0x46, 0x0c, 0x00, 0xfc, 
	0x07, 0x00, 0xb8, 0x03, 0x00, 0x00, 0x00, 0x00,
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
    switch (i) {
        case dm::ICON_MEASURE_LG:  return xbm_measure_lg;
        case dm::ICON_TIME_LG:     return xbm_time_lg;
        case dm::ICON_WEATHER_LG:  return xbm_weather_lg;
        case dm::ICON_WEATHER_SUN_LG:   return xbm_weather_sun_lg;
        case dm::ICON_WEATHER_RAIN_LG:  return xbm_weather_rain_lg;
        case dm::ICON_WEATHER_STORM_LG: return xbm_weather_storm_lg;
        case dm::ICON_WEATHER_SNOW_LG:  return xbm_weather_snow_lg;
        case dm::ICON_LOCATE_LG:   return xbm_locate_lg;
        case dm::ICON_LED_LG:      return xbm_led_lg;
        case dm::ICON_INTERVAL_LG: return xbm_interval_lg;
        case dm::ICON_STATS_LG:    return xbm_stats_lg;
        case dm::ICON_WIFIMENU_LG: return xbm_wifimenu_lg;
        case dm::ICON_RESET_LG:    return xbm_reset_lg;
        case dm::ICON_SLEEP_LG:    return xbm_sleep_lg;
        case dm::ICON_PORTAL_LG:   return xbm_portal_lg;
        case dm::ICON_CLEAR_LG:    return xbm_clear_lg;
        case dm::ICON_SELECT_LG:   return xbm_select_lg;
        case dm::ICON_BACK_LG:     return xbm_back_lg;
        case dm::ICON_ROOM_LG:     return xbm_room_lg;
        case dm::ICON_TREND_LG:    return xbm_trend_lg;
        default:                   return xbm_measure_lg;
    }
}

// ---- Toast state ----------------------------------------------------------
struct ToastState {
    char     text[24];
    uint32_t startMs;
    uint16_t holdMs;
    uint8_t  phase;   // 0=idle, 1=slide-in, 2=hold, 3=slide-out
} g_toast = { {0}, 0, 0, 0 };

// ---- Font resolution ------------------------------------------------------
// setFontPosTop() is applied globally so y = top-of-glyph (Adafruit-compat).
static const uint8_t *resolve_font(dm::Font f) {
    switch (f) {
        case dm::FONT_SMALL:  return u8g2_font_5x7_tf;
        case dm::FONT_NORMAL: return u8g2_font_6x10_tf;
        case dm::FONT_LARGE:  return u8g2_font_10x20_tf;
        // FONT_HUGE was logisoso28 (28 px tall). On 64x48 with a 10 px header
        // that leaves only 38 px vertical which the tall glyphs overflow. Use
        // logisoso18 (18 px) instead — numeric only, fits comfortably, still
        // reads as a "big digit" font.
        case dm::FONT_HUGE:   return u8g2_font_logisoso18_tn;
    }
    return u8g2_font_6x10_tf;
}

// ---- I2C address probe ----------------------------------------------------
static bool probe_i2c(uint8_t addr7) {
    Wire.beginTransmission(addr7);
    return Wire.endTransmission() == 0;
}

}  // namespace

namespace dm {

// ---------------------------------------------------------------------------
bool Animation::tick() {
    uint32_t now = millis();
    if (lastUpdateMs == 0) {
        lastUpdateMs = now;
        return false;
    }
    if (now - lastUpdateMs >= intervalMs) {
        lastUpdateMs = now;
        frame++;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
bool init(SemaphoreHandle_t sharedI2cMutex) {
    g_i2cMutex = sharedI2cMutex;
    uint8_t addr7 = 0;
    if (probe_i2c(0x3C))      addr7 = 0x3C;
    else if (probe_i2c(0x3D)) addr7 = 0x3D;
    else                      return false;

    g_u8g2.setI2CAddress(addr7 << 1);
    if (!g_u8g2.begin()) return false;
    g_u8g2.setFontPosTop();
    g_u8g2.setFontMode(0);           // solid background
    g_u8g2.setDrawColor(1);
    g_u8g2.clearBuffer();
    g_u8g2.sendBuffer();
    g_ready = true;
    return true;
}

bool ready() { return g_ready; }

void hardClear() {
    if (!g_ready) return;
    if (g_i2cMutex && xSemaphoreTake(g_i2cMutex, portMAX_DELAY) != pdTRUE) return;
    g_u8g2.clearBuffer();
    g_u8g2.sendBuffer();
    if (g_i2cMutex) xSemaphoreGive(g_i2cMutex);
}

bool beginFrame(uint32_t timeoutMs) {
    if (!g_ready) return false;
    if (g_i2cMutex && xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) return false;
    g_u8g2.clearBuffer();
    g_u8g2.setDrawColor(1);
    g_inverted = false;
    g_dirty = true;   // conservative: assume something will be drawn
    g_frameActive = true;
    return true;
}

void endFrame() {
    if (!g_frameActive) return;
    if (g_dirty) g_u8g2.sendBuffer();
    g_dirty = false;
    g_frameActive = false;
    if (g_i2cMutex) xSemaphoreGive(g_i2cMutex);
}

void markDirty() { g_dirty = true; }

// ---- primitives -----------------------------------------------------------
void setFont(Font f) { g_u8g2.setFont(resolve_font(f)); }
int  textWidth(const char *s) { return s ? g_u8g2.getStrWidth(s) : 0; }
int  fontAscent() { return g_u8g2.getAscent(); }

void drawText(int x, int y, const char *s) {
    if (!s) return;
    g_u8g2.setDrawColor(1);
    g_u8g2.drawStr(x, y, s);
}

void drawTextInverted(int x, int y, const char *s) {
    if (!s) return;
    g_u8g2.setDrawColor(0);
    g_u8g2.drawStr(x, y, s);
    g_u8g2.setDrawColor(1);
}

void drawIcon(int x, int y, Icon icon) {
    // 12x12 XBM
    g_u8g2.setDrawColor(1);
    g_u8g2.drawXBMP(x, y, 12, 12, icon_xbm(icon));
}

void drawPixel(int x, int y)               { g_u8g2.setDrawColor(1); g_u8g2.drawPixel(x, y); }
void drawHLine(int x, int y, int w)        { g_u8g2.setDrawColor(1); g_u8g2.drawHLine(x, y, w); }
void drawVLine(int x, int y, int h)        { g_u8g2.setDrawColor(1); g_u8g2.drawVLine(x, y, h); }
void drawLine(int x0, int y0, int x1, int y1) {
    g_u8g2.setDrawColor(1); g_u8g2.drawLine(x0, y0, x1, y1);
}
void drawRect(int x, int y, int w, int h)  { g_u8g2.setDrawColor(1); g_u8g2.drawFrame(x, y, w, h); }
void drawFilledRect(int x, int y, int w, int h) {
    g_u8g2.setDrawColor(1); g_u8g2.drawBox(x, y, w, h);
}
void clearRect(int x, int y, int w, int h) {
    g_u8g2.setDrawColor(0); g_u8g2.drawBox(x, y, w, h); g_u8g2.setDrawColor(1);
}
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

// ---- composed helpers -----------------------------------------------------
void drawHeader(int originX, int originY, int width,
                const char *title, bool showIcon, Icon icon) {
    drawFilledRect(originX, originY, width, 10);
    setFont(FONT_SMALL);
    drawTextInverted(originX + 3, originY + 2, title);
    if (showIcon) {
        // Proper 8x8 wifi glyph fits exactly inside the 10-px header, no
        // clipping and no bleed into the body area below.
        int ix = originX + width - 10;
        int iy = originY + 1;
        g_u8g2.setDrawColor(0);
        g_u8g2.drawXBMP(ix, iy, 8, 8, xbm_wifi_hdr8);
        g_u8g2.setDrawColor(1);
    }
}

void drawMenuRow(int originX, int y, int width, int rowHeight,
                 const char *text, bool selected) {
    setFont(FONT_NORMAL);
    int textY = y + (rowHeight - 8) / 2;
    if (textY < y) textY = y;
    if (selected) {
        drawFilledRect(originX, y, width, rowHeight);
        drawTextInverted(originX + 2, textY, text);
    } else {
        drawText(originX + 2, textY, text);
    }
}

void drawProgressBar(int x, int y, int w, int h, uint8_t percent) {
    if (percent > 100) percent = 100;
    drawRect(x, y, w, h);
    int fill = (int)((w - 2) * (percent / 100.0f) + 0.5f);
    if (fill > 0) drawFilledRect(x + 1, y + 1, fill, h - 2);
}

void showStatus(int originX, int originY, int width,
                const char *header, const char *line1, const char *line2,
                const char *line3, bool showIcon, Icon icon) {
    // portMAX_DELAY matches legacy updateOLED() semantics.
    if (!beginFrame(portMAX_DELAY)) return;
    drawHeader(originX, originY, width, header, showIcon, icon);
    // 64x48-friendly layout: three FONT_NORMAL rows below the 10px header.
    setFont(FONT_NORMAL);
    if (line1 && line1[0]) drawText(originX + 2, originY + 14, line1);
    if (line2 && line2[0]) drawText(originX + 2, originY + 25, line2);
    if (line3 && line3[0]) drawText(originX + 2, originY + 36, line3);
    endFrame();
}

// ---------------------------------------------------------------------------
// Tween
// ---------------------------------------------------------------------------
float easeOutCubic(float x) {
    if (x <= 0.f) return 0.f;
    if (x >= 1.f) return 1.f;
    float inv = 1.f - x;
    return 1.f - inv * inv * inv;
}
float easeInOutCubic(float x) {
    if (x <= 0.f) return 0.f;
    if (x >= 1.f) return 1.f;
    return (x < 0.5f) ? (4.f * x * x * x)
                      : (1.f - powf(-2.f * x + 2.f, 3.f) / 2.f);
}

void Tween::start_(float from, float to, uint16_t durationMs) {
    start = from;
    end   = to;
    durMs = durationMs == 0 ? 1 : durationMs;
    t0Ms  = millis();
    running = true;
}
float Tween::value() {
    if (!running) return end;
    uint32_t now = millis();
    uint32_t dt  = now - t0Ms;
    if (dt >= durMs) {
        running = false;
        return end;
    }
    float x = (float)dt / (float)durMs;
    return start + (end - start) * easeOutCubic(x);
}

// ---------------------------------------------------------------------------
// Icon 24x24
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Raw buffer access + horizontal shifted blit
// ---------------------------------------------------------------------------
uint8_t *rawBuffer()    { return g_u8g2.getBufferPtr(); }
size_t   rawBufferLen() { return (size_t)g_u8g2.getBufferTileWidth() * g_u8g2.getBufferTileHeight() * 8; }
int      bufferWidth()  { return g_u8g2.getBufferTileWidth() * 8; }
int      bufferHeight() { return g_u8g2.getBufferTileHeight() * 8; }

// SSD1306 buffer layout: pages of 8 vertical pixels stacked; within a page,
// consecutive bytes step 1 pixel horizontally. Shifting horizontally means
// shifting bytes within each page.
void blitShifted(const uint8_t *src, int dxPixels) {
    int W = bufferWidth();
    int H = bufferHeight();
    int pages = H / 8;
    uint8_t *dst = rawBuffer();
    int stride = g_u8g2.getBufferTileWidth() * 8;
    for (int p = 0; p < pages; p++) {
        for (int x = 0; x < W; x++) {
            int srcX = x - dxPixels;
            uint8_t v = 0;
            if (srcX >= 0 && srcX < W) v = src[p * stride + srcX];
            dst[p * stride + x] = v;
        }
    }
}

// ---------------------------------------------------------------------------
// Animated text-menu (scrolling strip with XOR highlight band)
// ---------------------------------------------------------------------------
void drawMenuAnimated(int menuY, int rowH, int visibleRows,
                      const char *const *items, int itemCount,
                      float fractionalIdx) {
    setFont(FONT_NORMAL);
    // Middle row is the selection band.
    int midRow = visibleRows / 2;
    int highlightY = menuY + midRow * rowH;

    // Render 2 rows above and below the visible viewport to guarantee coverage
    // during animation.
    int base = (int)floorf(fractionalIdx);
    float frac = fractionalIdx - base;
    int textInset = 2;
    int textYOffset = (rowH - 8) / 2;
    for (int rel = -2; rel <= visibleRows + 2; rel++) {
        int i = ((base + rel - midRow) % itemCount + itemCount) % itemCount;
        int y = menuY + (rel - frac) * rowH;
        if (y > bufferHeight() || y + rowH < menuY) continue;
        setFont(FONT_NORMAL);
        drawText(textInset, y + textYOffset, items[i]);
    }

    // XOR selection band — flips whatever text is under it, giving the visual
    // impression that the highlight is stationary while the strip scrolls.
    g_u8g2.setDrawColor(2);          // 2 = XOR
    g_u8g2.drawBox(0, highlightY, bufferWidth(), rowH);
    g_u8g2.setDrawColor(1);
}

// ---------------------------------------------------------------------------
// Icon cover-flow menu (Phase 4)
// ---------------------------------------------------------------------------
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
    int labelY = iconY + 26;
    setFont(FONT_SMALL);
    int cur  = ((base) % count + count) % count;
    int nxt  = ((base + 1) % count + count) % count;
    // Text opacity trick: draw current shifted left by frac*W, next shifted
    // right, so the text slides in sync with the icons.
    if (labels && labels[cur]) {
        int wCur = textWidth(labels[cur]);
        int xCur = centreX - wCur / 2 - (int)(frac * W);
        drawText(xCur, labelY, labels[cur]);
    }
    if (labels && labels[nxt]) {
        int wNxt = textWidth(labels[nxt]);
        int xNxt = centreX - wNxt / 2 + (int)((1.f - frac) * W);
        drawText(xNxt, labelY, labels[nxt]);
    }
}

// ---------------------------------------------------------------------------
// Toast (non-blocking banner)
// ---------------------------------------------------------------------------
static const uint16_t TOAST_SLIDE_MS = 150;
static const int      TOAST_H        = 12;

void toast(const char *text, uint16_t holdMs) {
    if (!text) return;
    // If a toast is showing, just replace (simple pre-emptive queue).
    strncpy(g_toast.text, text, sizeof(g_toast.text) - 1);
    g_toast.text[sizeof(g_toast.text) - 1] = 0;
    g_toast.startMs = millis();
    g_toast.holdMs  = holdMs;
    g_toast.phase   = 1;
}

bool toastTick() {
    if (g_toast.phase == 0) return false;
    uint32_t t = millis() - g_toast.startMs;
    int y = -TOAST_H;
    if (t < TOAST_SLIDE_MS) {
        // slide in
        float p = easeOutCubic((float)t / TOAST_SLIDE_MS);
        y = -TOAST_H + (int)(p * TOAST_H);
        g_toast.phase = 1;
    } else if (t < TOAST_SLIDE_MS + g_toast.holdMs) {
        y = 0;
        g_toast.phase = 2;
    } else if (t < TOAST_SLIDE_MS * 2u + g_toast.holdMs) {
        float p = easeOutCubic((float)(t - TOAST_SLIDE_MS - g_toast.holdMs) / TOAST_SLIDE_MS);
        y = 0 - (int)(p * TOAST_H);
        g_toast.phase = 3;
    } else {
        g_toast.phase = 0;
        return false;
    }
    // Render on top of current buffer.
    g_u8g2.setDrawColor(1);
    g_u8g2.drawBox(0, y, bufferWidth(), TOAST_H);
    g_u8g2.setDrawColor(0);
    setFont(FONT_SMALL);
    int w = g_u8g2.getStrWidth(g_toast.text);
    int tx = (bufferWidth() - w) / 2;
    if (tx < 1) tx = 1;
    g_u8g2.drawStr(tx, y + 3, g_toast.text);
    g_u8g2.setDrawColor(1);
    return true;
}

// ---------------------------------------------------------------------------
// Boot splash — blocking ~900ms
// ---------------------------------------------------------------------------
void bootSplash() {
    if (!g_ready) return;
    const uint32_t T_DOT = 300, T_SWEEP = 300, T_SUB = 300;
    uint32_t t0 = millis();

    // Phase 1: expanding dot centre 0..300ms
    while (millis() - t0 < T_DOT) {
        if (!beginFrame(50)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }
        float p = easeOutCubic((float)(millis() - t0) / T_DOT);
        int r = (int)(p * 10.f);
        drawFilledCircle(bufferWidth() / 2, bufferHeight() / 2, r);
        endFrame();
        vTaskDelay(pdMS_TO_TICKS(16));
    }

    uint32_t t1 = millis();
    // Phase 2: AETHER sweep-reveal
    setFont(FONT_LARGE);
    while (millis() - t1 < T_SWEEP) {
        if (!beginFrame(50)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }
        float p = easeOutCubic((float)(millis() - t1) / T_SWEEP);
        setFont(FONT_LARGE);
        int W = bufferWidth();
        int w = g_u8g2.getStrWidth("AETHER");
        int tx = (W - w) / 2;
        int ty = bufferHeight() / 2 - 10;
        drawText(tx, ty, "AETHER");
        // Reveal wipe: clear right portion.
        int revealed = (int)(p * W);
        if (revealed < W) {
            clearRect(revealed, ty - 2, W - revealed, 22);
        }
        endFrame();
        vTaskDelay(pdMS_TO_TICKS(16));
    }

    uint32_t t2 = millis();
    // Phase 3: subtitle "v2.0" dither fade-in
    while (millis() - t2 < T_SUB) {
        if (!beginFrame(50)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }
        setFont(FONT_LARGE);
        int W = bufferWidth();
        int w = g_u8g2.getStrWidth("AETHER");
        int tx = (W - w) / 2;
        int ty = bufferHeight() / 2 - 10;
        drawText(tx, ty, "AETHER");

        setFont(FONT_SMALL);
        const char *sub = "v2.0";
        int sw = g_u8g2.getStrWidth(sub);
        int sx = (W - sw) / 2;
        int sy = ty + 22;
        drawText(sx, sy, sub);
        // Dither mask: overlay a checker pattern with density inverse to progress
        float p = easeOutCubic((float)(millis() - t2) / T_SUB);
        int mask = (int)((1.f - p) * 4.f);
        if (mask > 0) {
            g_u8g2.setDrawColor(0);
            for (int y = sy - 1; y < sy + 8; y++) {
                for (int x = sx - 1; x < sx + sw + 1; x++) {
                    if (((x + y) & mask) == 0) g_u8g2.drawPixel(x, y);
                }
            }
            g_u8g2.setDrawColor(1);
        }
        endFrame();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

}  // namespace dm
