# Time Screen Redesign — Design Spec

**Date:** 2026-07-09
**Status:** Approved by user, ready for implementation planning

## Context

The Time screen (`drawClockScreen()` / `showTimePage()` in `firmware/src/main.cpp`)
already went through earlier fixes this session (colon-blink glyph-width bug), but
the user wanted to explore fresh visual directions purely out of curiosity, not to
fix a specific complaint. Several concepts were mocked up in the browser at true
64x48 scale; the approved direction combines elements from two of them:

- A **minimal/huge time** treatment (time dominates the screen, no header bar)
- A **day/night sun-or-moon icon accent** (Nest Hub/Echo Show-style ambient clock
  precedent), changing based on the current hour
- A **proportioned info strip** below the time showing day + date (Pebble/G-Shock
  watch-face-style treatment — a designed element, not tiny corner text)

Seconds are no longer displayed as text or a progress bar in the new design (the
mockups the user approved didn't include them) — the colon-blink still uses the
seconds value internally for its every-other-second toggle timing, it's just not
rendered as a visible number anymore.

## Layout

**No header bar** on this screen specifically (explicitly confirmed with the
user) — this is a deliberate exception to the convention every other screen
follows, freeing the full 48px height for the new composition. Day identification
(previously the header's title) moves to the new footer info strip instead.

From top to bottom:

1. **Sun/moon icon**: a small icon in a **dedicated reserved row at the very
   top** (not beside the time digits) — this avoids a real collision risk, since
   the huge time font already runs close to full 64px width when centred, and
   sharing a row with a corner icon risked overlapping the rightmost digit.
   Drawn procedurally (no bitmap asset) using existing circle/line primitives —
   the same technique already used for the sleep animation's crescent moon
   earlier this session:
   - **Daytime** (hour 06:00–17:59): a small filled circle with 8 short rays
     radiating outward (sun).
   - **Nighttime** (hour 18:00–05:59): a small filled circle with an offset
     circle "carved out" in background colour (crescent moon).
2. **Huge HH:MM**, same fixed-offset three-draw-calls technique already fixed
   earlier this session (HH, then conditionally ":", then MM, at pre-computed
   cumulative x-offsets) — unchanged logic, only its vertical position shifts
   down slightly to make room for the icon row above it.
3. **Thin divider line** separating the time from the info strip.
4. **Info strip**: day name on the left (e.g. "MON"), date on the right in a
   friendlier format than the previous numeric "09/07" — day-of-month + month
   abbreviation (e.g. "09 JUL"), both in `FONT_SMALL`, sized/positioned as a
   deliberate design element rather than tiny incidental text.

## Data changes

`drawClockScreen()`'s signature gains one new parameter: the raw 24-hour hour
value (`int hour24`), needed to decide sun vs. moon. `showTimePage()` already has
this available directly from the `struct tm` it already populates (`tinfo.tm_hour`)
— no new time-fetching logic needed, just pass the existing field through.

The date string format changes from `strftime(..., "%d/%m", &tinfo)` to
`strftime(..., "%d %b", &tinfo)` (e.g. "09 Jul") and gets uppercased to "09 JUL"
to match the existing day-name uppercasing convention already used in this
function.

## Non-goals

- No changes to the underlying NTP sync / timezone logic in `showTimePage()`.
- No changes to any other screen.
- No seconds-based UI element (no visible seconds digits, no progress bar/ring)
  in the new design — this was a deliberate simplification the user approved via
  the mockups, not an oversight.

## Testing / verification

No test runner exists for firmware (per project conventions) — verification is
`pio run` (build must succeed) plus manual hardware confirmation of: layout has
no header bar, sun/moon icon shows in its own row without colliding with the
time digits, time digits are correctly centred and the colon still blinks
correctly (no digit-shifting regression), divider and info strip render
correctly, and the icon correctly switches between sun and moon around the
06:00/18:00 boundaries. Given real font-metric uncertainty (exact pixel heights
are approximate per this codebase's existing comments), this joins the other
outstanding manual hardware-verification items from this session, and minor
y-position tuning during that verification is expected and acceptable.
