# UI Redesign Spec — Full 128×64 (post-U8g2 migration)

Status: **draft — Stage C**. Stages A/B keep the current 64×48 viewport at offset (32,16) so the backend swap is visually neutral. This document defines the target layout for Stage C, where OLED_OFFSET_X/Y/OLED_W/H are removed and every pixel is used.

## Global conventions

- Panel: 128 × 64 monochrome (SSD1306).
- Origin: top-left (0, 0). Y grows downward.
- Text baseline: `u8g2.setFontPosTop()` set once in `dm::init()` so `y` is the top of the glyph (matches the Adafruit convention already used in the codebase).
- Colors: 1 = foreground (white / lit pixel), 0 = background (black).
- All screens share a **12 px header bar** at `y = 0..11`, drawn inverted (fillRect 128×12 then black text).

## Font table

| Alias        | U8g2 font                    | Cap px | Cell (advance) | Used for                    |
|--------------|------------------------------|-------:|---------------:|-----------------------------|
| FONT_SMALL   | `u8g2_font_5x7_tf`           |     5  | 5×7            | header, footnotes           |
| FONT_NORMAL  | `u8g2_font_6x10_tf`          |     7  | 6×10           | menus, body                 |
| FONT_LARGE   | `u8g2_font_10x20_tf`         |    13  | 10×20          | values, spinner replacement |
| FONT_HUGE    | `u8g2_font_logisoso28_tn`    |    28  | ~19×28         | clock HH:MM only (digits)   |

Small font is used inside the header (12 px bar). Body area is `y = 14..63` (50 px).

## Icon set (XBM, 12×12)

| Alias        | Purpose                             |
|--------------|-------------------------------------|
| ICON_WIFI    | Header connected indicator          |
| ICON_CLOUD   | Sync success indicator              |
| ICON_PIN     | Location saved indicator            |
| ICON_SCAN    | Sensor scan running indicator       |

Placed right-aligned in the header: `x = 128 - 14, y = 0`.

## Screens

### Header bar (all screens)
- `fillRect(0, 0, 128, 12)` inverted.
- `drawText(2, 2, title)` in FONT_SMALL.
- Optional icon at `(114, 0)`.

### Main menu (`drawMenu`)
- 3 visible rows × 16 px each starting at `y = 14`.
- Row: `fillRect(0, y, 128, 16)` if selected + inverted text; otherwise plain text.
- Text: FONT_NORMAL at `(4, y + 3)`.
- Bottom auto-sleep progress bar: `fillRect(2, 62, mapped, 2)`.

### Status screen (`updateOLED` / `dm::showStatus`)
- Header bar with optional icon.
- Line 1 (FONT_LARGE) at `(4, 18)`.
- Line 2 (FONT_NORMAL) at `(4, 40)`.
- Line 3 (FONT_SMALL) at `(4, 54)` if present.

### Connecting / Syncing / Locating / Scanning (`uiTask` spinner)
- Header bar.
- `uiLine1` (FONT_NORMAL) at `(4, 18)`.
- `uiLine2` (FONT_NORMAL) at `(4, 34)`.
- Spinner FONT_LARGE at `(108, 34)` (right side, glyph rotates via animation frame).
- `uiLine3` (FONT_SMALL) at `(4, 54)` if present.

### Portal (`SS_PORTAL`)
- Header "PORTAL".
- `CONNECT TO:` at `(4, 18)` FONT_NORMAL.
- `AETHER_CFG` at `(4, 32)` FONT_LARGE.
- `192.168.4.1` at `(4, 52)` FONT_SMALL.

### WiFi sub-menu (`SS_WIFI_MENU`)
- Header "WIFI CONFIG".
- Same 3-row × 16 px menu layout as main menu.

### WiFi slot viewer (`showSavedWiFi`)
- Header `SLOT n [*]`.
- SSID view: `SSID:` FONT_SMALL at `(4, 16)`, value FONT_NORMAL at `(4, 28)`, `[HOLD] SET` FONT_SMALL at `(4, 54)`.
- PW view: `P:xxxx` FONT_NORMAL at `(4, 24)`, `[CLICK] BK` FONT_SMALL at `(4, 54)`.

### Clock
- Header `CLOCK <city>`.
- Time HH:MM FONT_HUGE centered at `y = 20`.
- Date + day FONT_SMALL at `(4, 54)`.

### Weather
- Header `WEATHER`.
- Temp FONT_HUGE-truncated at `(4, 18)`.
- Description FONT_NORMAL at `(4, 46)`.

### Locate (result)
- Header `LOCATE`.
- Pin icon at `(58, 16)` (24×24 large-icon variant, optional).
- City FONT_LARGE centered at `y = 40`.

### Stats
- Header `STATS`.
- 2×2 grid of KPI labels + values (FONT_SMALL labels, FONT_NORMAL values), 64×26 per cell.

### Deep sleep animation (`enterDeepSleep`)
- Header `SLEEPING...`.
- Power icon (radius 12) pulsing at center `(64, 32)`.
- Progress bar `drawRect(24, 54, 80, 4)` + `fillRect(24, 54, mapped, 4)`.

## Coordinate migration table (Stage B → Stage C)

Stage B keeps `OLED_OFFSET_X = 32, OLED_OFFSET_Y = 16, OLED_W = 64, OLED_H = 48`. Stage C zeroes the offsets and doubles W/H:

| Symbol         | Stage A/B | Stage C |
|----------------|----------:|--------:|
| OLED_OFFSET_X  |        32 |       0 |
| OLED_OFFSET_Y  |        16 |       0 |
| OLED_W         |        64 |     128 |
| OLED_H         |        48 |      64 |

Because every draw call funnels through `dm::` with these macros as inputs, Stage C is a pure macro/coordinate edit plus font upgrades.
