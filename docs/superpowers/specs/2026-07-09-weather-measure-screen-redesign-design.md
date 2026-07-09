# Weather & Measure Screen Redesign — Design Spec

**Date:** 2026-07-09
**Status:** Approved by user, ready for implementation planning

## Context

The Weather result screen (`drawWeatherScreen()`) and the per-sample Measure scan
screen (`drawMeasureSample()`), both in `firmware/src/main.cpp`, currently use the
exact same generic two-column layout as the unrelated Stats screen (big number +
small label + vertical divider line). Neither shows the icon it already has defined
(`ICON_WEATHER_LG` sun+cloud, `ICON_MEASURE_LG` thermometer) — those icons exist only
in the main menu, never on the actual result screens. This redesign gives both
screens their own visual identity using icons, decided via browser-simulated
mockups at true 64x48 OLED scale during brainstorming.

## Layout

Both screens adopt the same new composition ("icon left, stats stacked right"):

- **Left column** (~20px wide): a 24x24 icon, vertically centred in the content
  area (between the 12px header and, for Measure, the 9px footer).
- **Right column**: two stacked text lines.
  - **Hero line** (larger font): temperature, formatted as `<value>C` (e.g. `24C`)
    — no degree symbol (not used anywhere else in this firmware, avoiding an
    unverified font-glyph risk).
  - **Secondary line** (smaller font): humidity, formatted as `HUM <value>%`
    (e.g. `HUM 55%`).
- **Footer** (9px inverted bar): unchanged behavior per screen —
  - Weather: **removed entirely**. The condition text it used to show (e.g.
    "Clouds") is no longer needed since the icon now conveys the condition.
  - Measure: **unchanged** — still shows `LDR <value>` exactly as today.

## Weather icon mapping

OpenWeatherMap's `/data/2.5/weather` endpoint's `weather[0].main` field returns one
of a fixed set of strings. Map to icons as follows:

| `main` value | Icon |
|---|---|
| `Clear` | new sun icon (`ICON_WEATHER_SUN_LG`) |
| `Clouds` | existing cloud icon (`ICON_WEATHER_LG`, unchanged) |
| `Rain`, `Drizzle` | new rain icon (`ICON_WEATHER_RAIN_LG`) |
| `Thunderstorm` | new storm icon (`ICON_WEATHER_STORM_LG`) |
| `Snow` | new snowflake icon (`ICON_WEATHER_SNOW_LG`) |
| anything else (`Mist`, `Smoke`, `Haze`, `Dust`, `Fog`, `Sand`, `Ash`, `Squall`, `Tornado`) | falls back to `ICON_WEATHER_LG` |

A new function, `resolveWeatherIcon(const String &main)`, performs this mapping and
returns a `dm::Icon`.

**Icon sourcing**: the 4 new icons (sun, rain, storm, snowflake) are NOT
hand-authored by the agent — the user sources/converts them externally (same
image2cpp → 24x24 XBM workflow used for the Time icon earlier this session) and
provides the byte arrays. Until those are provided, the plan's icon-array tasks
are blocked on the user supplying the byte data; the plan documents exactly which
enum names and array names to use so the user knows what to reference.

**Measure icon**: unchanged — always `ICON_MEASURE_LG` (existing thermometer icon,
no per-condition variation, no new icon needed).

## Code changes

- **`firmware/src/display_manager.h`**: add 4 new `Icon` enum values
  (`ICON_WEATHER_SUN_LG`, `ICON_WEATHER_RAIN_LG`, `ICON_WEATHER_STORM_LG`,
  `ICON_WEATHER_SNOW_LG`) after the existing `ICON_WEATHER_LG` entry, before
  `ICON_COUNT`.
- **`firmware/src/display_manager.cpp`**: add 4 new `static const uint8_t
  xbm_weather_sun_lg[]` (etc.) PROGMEM arrays (placeholder/empty until the user
  supplies real byte data — the plan will flag this explicitly, no fabricated
  bitmap content), and add 4 new `case` entries to the `icon24_xbm()` switch.
- **`firmware/src/main.cpp`**:
  - `drawWeatherScreen()` signature changes from
    `(float tempC, int humPct, const char *desc)` to
    `(float tempC, int humPct, dm::Icon conditionIcon)` — drops the text
    description entirely, takes the resolved icon instead.
  - New `resolveWeatherIcon(const String &main)` function (mapping table above).
  - `showWeatherPage()`'s call site updates to resolve the icon from
    `doc["weather"][0]["main"]` and pass it to `drawWeatherScreen()` instead of
    the uppercased description string. The `desc.toUpperCase()` line and its
    use are removed since the raw (non-uppercased) `main` string is now what
    gets matched against the mapping table above (OpenWeatherMap's `main` field
    is always already capitalized-mixed-case as shown in the table, e.g.
    `"Clouds"`, not all-caps).
  - `drawMeasureSample()` rewritten to the new icon-left/stats-right layout,
    keeping its existing signature (`sampleIdx, totalSamples, tempC, humPct,
    ldr`) — no signature change needed since it always uses the same fixed
    `ICON_MEASURE_LG` icon (no per-call icon parameter required).
  - The shared `drawColumnValue()` helper is no longer used by either of these
    two screens after this change (it remains in use by `drawStatsScreen()`,
    which is NOT part of this redesign and stays as-is).

## Non-goals

- No changes to `drawStatsScreen()` (uses the same old two-column layout
  intentionally, out of scope for this redesign).
- No changes to the Measure-flow's glitch screen (`drawMeasureGlitch()`) or the
  weather-fetch error screen (`drawErrorScreen()` calls inside `showWeatherPage()`)
  — those are separate error-path screens, not the normal-path layout being
  redesigned here.
- No changes to how weather data is fetched, parsed, or how measurement sampling
  works — this is a rendering-only change.

## Testing / verification

No test runner exists for firmware (per project conventions). Verification is
`pio run` (build must succeed) plus manual visual confirmation on hardware once
the user has supplied the 4 new weather-condition icon byte arrays (until then,
the code will build with placeholder/empty icon data that renders as blank
squares — this is expected and clearly flagged in the implementation plan, not a
bug). Final visual confirmation joins the other outstanding manual
hardware-verification items from earlier in this session.
