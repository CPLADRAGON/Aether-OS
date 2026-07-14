# README Showcase Redesign

## Goal

Replace the current implementation-heavy README with a product-led showcase that
accurately represents the current Aether-OS firmware and dashboard.

The README must not use the outdated physical OLED photos or older dashboard
screenshot. It should look official, make the system understandable quickly, and
still provide developers with accurate setup and feature reference material.

## Audience

- A visitor evaluating the project from GitHub
- A maker who wants to build or extend it
- A user who wants a concise map of every device function

## Visual Strategy

### Fresh dashboard screenshot

Capture a current dashboard image locally from the actual Next.js app after
starting the dashboard with its existing environment configuration. Use it as the
primary proof that the web product is live and current.

### Simulated OLED previews

Create original, deterministic SVG/PNG interface previews for the current
64x48 monochrome OLED layout. These are interface previews, not claims of
physical-device photography.

The simulations must use the current firmware's actual text, menu order,
navigation model, and layout hierarchy:

1. Main menu / cover-flow
2. Measure
3. Time main + Time Detail
4. Weather main + Weather Detail
5. Room main + Room Status
6. Timer
7. Sleep / timer-complete alert

Use a curated gallery in the README. Do not render every menu item as an
individual image.

## README Structure

1. **Hero**
   - `AETHER_OS`
   - Single-sentence positioning: ESP32 room monitor with an OLED interface,
     cloud telemetry, dashboard, and Telegram control.
   - Project badges only where they are factual and stable.

2. **Product preview**
   - Fresh dashboard screenshot
   - Short dashboard caption
   - OLED UI gallery of simulated current screens
   - Clear note that OLED visuals are interface previews

3. **What it does**
   - Environment sensing and local OLED operation
   - Deep sleep and Memory-Link WiFi
   - Offline-safe reading queue
   - Cloud dashboard/PWA/Telegram integration
   - Timer and alert behavior

4. **Complete function matrix**
   - Every main-menu item in current menu order
   - Columns: menu item, purpose, control behavior, network dependency
   - Include main behavior plus any detail subpage behavior
   - Include current menu order:
     `MEASURE`, `TIME`, `WEATHER`, `ROOM`, `LOCATE`, `TREND`, `TIMER`,
     `LED`, `INTERVAL`, `WIFI MENU`, `STATS`, `RESET STATS`, `DEEP SLEEP`

5. **System architecture**
   - Compact ESP32 -> Supabase -> Next.js dashboard / Telegram flow
   - Mention Core 0 UI and Core 1 monitoring as an implementation detail,
     without overwhelming the top of the README

6. **Quick start**
   - Clone
   - Apply Supabase schema and the `lux_value` migration where required
   - Configure firmware secrets and dashboard `.env.local`
   - Build/flash firmware
   - Run dashboard
   - Link detailed deployment instructions instead of duplicating them

7. **Technology and safety note**
   - ESP32/Arduino/PlatformIO
   - Next.js, Supabase, Telegram
   - Explicit warning that current Supabase RLS is permissive and needs auth
     before public/sensitive deployment

## Image Assets

New README-specific assets belong under `docs/pics/readme/`.

- `dashboard-current.png`
- `oled-menu.png`
- `oled-measure.png`
- `oled-time.png`
- `oled-weather.png`
- `oled-room.png`
- `oled-timer.png`
- `oled-alert.png`

Generated source files/scripts may live under `docs/pics/readme/source/` if
needed to make the previews reproducible. They are not application code.

## Non-goals

- Do not use outdated physical OLED photos.
- Do not add external screenshot-hosting or image-CDN dependencies.
- Do not rewrite `DEPLOYMENT.md`; link to it.
- Do not claim a physical behavior that has not been implemented.

## Validation

- Check every menu item and interaction in the matrix against `main.cpp`.
- Confirm all image paths render in GitHub Markdown.
- Confirm README commands match current `firmware/` and `dashboard/` scripts.
- Capture dashboard only from the current local app, not an old repository
  image.
