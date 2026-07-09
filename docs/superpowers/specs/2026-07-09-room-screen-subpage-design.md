# Room Screen Subpage Navigation — Design Spec

**Date:** 2026-07-09
**Status:** Approved by user, ready for implementation planning

## Context

The Room screen (`drawRoomStatus()` / `showRoomPage()` in `firmware/src/main.cpp`)
was recently restyled to an icon-left layout matching Weather/Measure, but its
footer (holding a combined comfort tag, e.g. "WARM+NORMAL") constrained the
content area to the same tight 27px budget as Measure — preventing it from using
Weather's more spacious 36px layout (bigger hero font, more breathing room).

This spec moves that footer content to a new subpage, freeing Room's main view to
match Weather's layout exactly, and expands the relocated content into a richer
three-line breakdown (temp/humidity/light status + raw values) rather than the
single combined tag it replaces.

## Main view changes

`drawRoomStatus()` adopts Weather's exact layout: house icon (`dm::ICON_ROOM_LG`)
left, vertically centred in the full 36px content area (y=12..47, no footer);
temperature as hero (`FONT_LARGE`) on the right, humidity as secondary
(`FONT_SMALL`) below it — same y-positions as `drawWeatherScreen()` (14, 36),
since the content area is now identically sized.

The light-level tag in the header's top-right corner (BRIGHT/DIM/DARK,
inverted text on the header bar) is **unchanged** — it lives in the header, not
the content area, so it isn't affected by removing the footer.

**New: a small "▸" chevron indicator** in the bottom-right corner of the main
view (a single small glyph, not a full row) hints that a subpage is available.

## New subpage: "Room Status"

A new screen, reached by short-tapping while on the Room main view. Shows three
labeled lines, each combining the existing status tag with its raw sensor value:

```
TEMP: WARM (24C)
HUM: NORMAL (52%)
LIGHT: DIM (812)
```

Header title: "STATUS" — distinguishes it from "ROOM" on the main view.
Uses the existing status-tag logic already computed in `showRoomPage()`
(`tempTag`, `humTag`, `lightTag` — currently combined into `comfortTag`, which
this subpage replaces the display of, though the underlying tag-computation
logic is unchanged). The raw LDR value (`ldrRaw`) is already available as a
parameter to the existing draw function and is reused here for the light line.

## Navigation model

Reuses the existing short-press/long-press detection pattern already used
elsewhere in this firmware (`isPressing` / `isrPressStart` / `LONG_PRESS_MS`,
see e.g. the button-poll loops in `showWeatherPage()`/`runMeasurementFlow()`),
applied within `showRoomPage()`'s existing wait loop:

- **Short tap while on Room main view** → switch to the Room Status subpage.
- **Short tap while on Room Status subpage** → switch back to Room main view.
- **Long press (≥`LONG_PRESS_MS`), from either view** → exit to the main menu
  (matching the existing behavior everywhere else in the firmware).
- **Timeout** (no interaction for the existing wait duration) → exit to the main
  menu, same as today's behavior when no button is pressed at all.

This is a toggle between exactly two views (no deeper navigation stack) — short
tap always cycles between Room main and Room Status; there's no "third" screen
to navigate through.

## Implementation approach

`showRoomPage()` currently calls `drawRoomStatus()` once, then
`waitWithButtonPoll(5000)`, then returns. This changes to a loop: draw whichever
view is currently active (main or subpage), wait for input distinguishing short
tap vs. long press vs. timeout, and either toggle the active view (short tap),
break out to the menu (long press or timeout), redrawing on each toggle. A new
small `drawRoomStatusDetail(...)` function (or similarly named) renders the
subpage; `drawRoomStatus()` gains the chevron indicator.

## Non-goals

- No changes to how sensor values are read or how status tags are computed
  (`tempTag`/`humTag`/`lightTag` logic in `showRoomPage()` stays the same).
- No changes to any other screen's navigation model — this pattern is scoped to
  Room only for now (not a general "all screens get subpages" change).
- No changes to the light-level header tag or its position/logic.

## Testing / verification

No test runner exists for firmware (per project conventions) — verification is
`pio run` (build must succeed) plus manual hardware confirmation of: main view
layout matches Weather, chevron indicator visible, short tap toggles to/from
the subpage correctly, subpage shows all three status+value lines without
overlap, long press from either view exits to the menu. Joins the other
outstanding manual hardware-verification items from this session.
