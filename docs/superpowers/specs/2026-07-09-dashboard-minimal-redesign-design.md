# Dashboard Redesign — "Minimal & Calm" — Design Spec

**Date:** 2026-07-09
**Scope:** Frontend only (`dashboard/src/**`, `dashboard/package.json`)
**Status:** Approved for planning

## Problem

The current dashboard uses a dense "cyberpunk HUD" aesthetic — neon cyan/magenta/lime glows, heavy glassmorphism blur on every panel, `UPPERCASE_WITH_UNDERSCORES` labels, dual sidebar+topbar navigation chrome, and a mix of four different font families (Lexend, Geist, Geist Mono, Space Grotesk). The user finds this "heavy and ugly" and wants a full visual redesign, with some structural simplification along the way.

## Goals

1. Replace the visual language with a minimal, calm style inspired by Linear/Vercel: neutral near-black palette, one accent color, flat cards (no blur/glow), Inter typeface throughout, sentence-case labels.
2. Simplify navigation from dual sidebar+topbar to a single top bar with inline tabs.
3. Remove the unused `Sensors` tab; fold its genuinely useful content (accelerometer reading, per-metric min/max/avg) into the Dashboard tab.
4. Drop the Battery Voltage stat entirely (currently fed by a hardcoded `battery_v = 3.3` in firmware — not real data, and misleading to display).
5. Fix a firmware bug found during this session: the Clock page's `:` blink was shifting the MM digits sideways (root cause: swapping `':'`→`' '` in a fixed-length string doesn't preserve visual width if the font's space and colon glyphs differ in advance width). **This is already fixed and committed (`b61168d`) — included here only for the record; no further action needed.**

## Non-goals

- No changes to data fetching, Supabase realtime subscription logic, or any backend/API route.
- No changes to the Comfort Index, presence-detection, or apparent-temperature calculation logic — only how their results are displayed.
- No changes to the Telegram bot.
- No dark/light mode toggle — dark-only, matching the existing product (and the reference styles: Linear/Vercel dashboards are dark-first).
- No new charting library — ECharts stays; only its color theme changes.

## Approach

Restyle in place rather than rewrite from scratch: every component keeps its existing props/interfaces and data flow, only the Tailwind classes / inline styles / ECharts theme options change. The two structural changes (nav simplification, Sensors merge) touch `page.tsx`, `Layout.tsx`, `Navigation.tsx`, and require moving a few calculations from `SensorsView.tsx` into `DashboardView.tsx` before deleting the former.

Rejected alternative: a from-scratch rewrite of the whole dashboard as a new component tree. Rejected because the existing component boundaries (`KPICard`, `TrendChart`, `ActivityTimeline`, `SystemLogs`, `SessionTable`, `DatePicker`) are already reasonably factored — a full rewrite would re-risk realtime/data-fetching bugs for no benefit over a targeted restyle + two structural moves.

## Design

### 1. Theme tokens (`dashboard/src/app/globals.css`)

Replace the `@theme inline` block and body/glass-panel utility classes:

```css
--color-background: #0d0d0f;
--color-surface: #16161a;
--color-surface-border: #1f1f23;
--color-accent: #818cf8;        /* indigo — replaces cyan/magenta/lime */
--color-text-primary: #f4f4f5;
--color-text-secondary: #a1a1aa;
--color-text-tertiary: #6b7280;
--color-status-online: #34d399; /* muted emerald, no glow */
--color-status-warn: #fbbf24;   /* muted amber */
--color-status-offline: #f87171;/* muted red */

--font-sans: "Inter", -apple-system, sans-serif;
--font-mono: "Geist Mono", monospace; /* kept ONLY for log/data text, not labels */
```

- Delete `.glass-panel`, `.glass-panel-heavy`, `.glow-cyan`, `.glow-magenta`, `.glow-lime`, `.active-glow` utility classes (no blur/glow anywhere in the new design).
- Replace with a single `.card` utility: `background: var(--color-surface); border: 1px solid var(--color-surface-border); border-radius: 8px;` — no backdrop-filter.
- Keep `.animate-pulse-dot` (still useful for the small "online" status dot) but drop its glow — pure opacity/scale pulse, no `box-shadow`.
- Remove `.progress-rainbow` (three-color animated gradient) — progress bars become a flat single-color fill in `--color-accent` (or the relevant muted status color when a value is out of nominal range).
- Remove `.custom-scrollbar`'s cyan thumb color — restyle to neutral gray (`rgba(255,255,255,0.15)`).

### 2. Typography

- All components switch from `font-headline` (Lexend)/`font-body` (Geist)/`font-space` (Space Grotesk) to a single `font-sans` (Inter), loaded via Next.js font loader in `layout.tsx` (replacing whatever multi-font setup is there now).
- `font-mono` (Geist Mono) is kept, but only for genuinely tabular/log content: `SystemLogs` message text, raw timestamps. Not for card labels, not for section headings.
- All labels currently using `uppercase tracking-widest`/`tracking-[0.3em]` (e.g. "TEMPERATURE", "SYSTEM_MONITORING_UNIT", "ENVIRONMENTAL_OVR") become normal sentence case with no letter-spacing hack: "Temperature", "Environmental Overview".

### 3. Navigation (`Layout.tsx`, `Navigation.tsx`)

- Delete the `<aside>` sidebar block entirely (currently ~40 lines in `Layout.tsx`, including the logo/version block, `NavItem` list, and the "System Logs" footer link).
- Delete the `<nav>` mobile bottom-navigation block (`MobileNavItem` usage) — no longer needed once only 2 tabs remain and the top bar handles tabs at all viewport widths.
- Single sticky top bar: logo/wordmark (left) → inline tab buttons "Dashboard" / "Power" (left-center) → connection status pill + avatar (right). On narrow viewports, tabs remain inline (2 tabs fit comfortably; no overflow menu needed).
- `Navigation.tsx` shrinks to one exported component, `TopTab`, replacing both `NavItem` and `MobileNavItem`. Props: `{ label: string; active: boolean; onClick: () => void }`.
- Status pill (`online`/`connecting`/`offline`) keeps its existing 3-state logic from `Layout.tsx`, restyled: muted background tint + small pulse dot, no border glow.

### 4. Content restructure — Sensors tab removed

- Delete `dashboard/src/components/SensorsView.tsx`.
- In `dashboard/src/app/page.tsx`:
  - `Tab` type narrows from `'dashboard' | 'sensors' | 'power'` to `'dashboard' | 'power'`.
  - Remove the `SensorsView` import and its conditional render branch.
  - Remove any state/props that existed solely to feed `SensorsView` (audit `page.tsx` for `sensors`-only state during implementation).
- In `dashboard/src/components/DashboardView.tsx`:
  - Add a compact stats section reusing the min/max/avg calculation currently in `SensorsView.tsx` (the `stats` `useMemo` block: min/max/avg over `temperature`, `humidity`, `ldr_value`, `accel_total` — **excluding** `battery_v`, which is dropped per Goal 4).
  - Add an "Accelerometer" reading (`latest.accel_total`) as a new KPI-style stat or a small inline stat card — motion/vibration data has genuine value (ties into presence detection) and isn't shown anywhere else after Sensors is removed.
  - **Placement**: a new "Sensor Details" row below the 4 existing KPI cards, containing 5 small stat chips (Temp min/max/avg, Humidity min/max/avg, Light min/max/avg, Accelerometer current + min/max/avg, laid out as a horizontally-scrollable row on mobile / a 4-5 column grid on desktop). This keeps the primary 4 KPI cards clean and glanceable while still surfacing the detail data — separating "current snapshot" (KPI row) from "period statistics" (detail row) is a clearer information hierarchy than interleaving min/max/avg into each KPI card.

### 5. Component restyle (data/props unchanged, visuals only)

- **`KPICard.tsx`**: remove the `colorConfig` glow/border-per-color system. New unified style: flat card, value in large Inter semibold in `--color-text-primary` (or a muted status color — amber/red — only when the value is genuinely out of a nominal range), thin 2px flat progress bar in `--color-accent`. The `color` prop simplifies from `'cyan' | 'magenta' | 'lime' | 'amber' | 'white'` to a semantic `'normal' | 'warn' | 'critical'`.
- **`TrendChart.tsx`** (ECharts): update the ECharts `option` color values — line/area colors move from cyan/magenta/lime to indigo (`--color-accent`) plus 1-2 muted neutrals (slate grays) for the secondary series; the `markArea` (currently a bright glow band for "dark periods") becomes a subtle `rgba(255,255,255,0.04)` band.
- **`ActivityTimeline.tsx`**: replace `bg-emerald-500/20`/`bg-red-500/20` glowing dots with flat muted-status-colored dots, no ring glow.
- **`SystemLogs.tsx`**: flat card, log level badges (`DEBUG`/`INFO`/`WARN`/`ERROR`) restyled as small muted pill labels instead of bright colored tags; monospace text kept for the log message itself.
- **`SessionTable.tsx`** (+ its `SessionVolatilityChart`/`UptimeAccumulationChart` exports): same flat-card + ECharts retheme treatment as `TrendChart`.
- **`DatePicker.tsx`**: flat card/dropdown, remove any blur/glow, restyle selected-state to a solid `--color-accent` background instead of a glow ring.
- **`PowerView.tsx`**: no structural change (it's already just KPI cards + charts + a table) — restyle only, following the same token/utility replacements as above.

### 6. Housekeeping

- Remove `recharts` from `dashboard/package.json` dependencies — confirmed zero imports anywhere in `dashboard/src/` (only `echarts`/`echarts-for-react` are actually used).

## Error handling / edge cases

- No new error states are introduced. Existing loading/empty states (`"No sensor data available yet."`, `"No recent activity shifts detected."`, loading spinner) are restyled in place using the new flat-card system — same conditions, same messages, new visual treatment.
- The accelerometer stat folded into Dashboard must handle the same "no readings yet" case `SensorsView` already handled (guard on `latest`/`stats` being null before rendering).

## Testing

This is a Next.js project with no existing test runner configured (per `CLAUDE.md`/`AGENTS.md`). Verification is manual: `npm run dev`, visually confirm both tabs (Dashboard, Power) render correctly with real Supabase data, confirm realtime updates still animate KPI values, confirm responsive layout at mobile width now that the bottom nav is gone, confirm `npm run build` succeeds with no unused-import errors after `SensorsView.tsx` deletion.

## Deliverables

- `dashboard/src/app/globals.css` — new theme tokens, flat `.card` utility, removed glow/blur utilities.
- `dashboard/src/app/layout.tsx` — Inter font loader (replacing multi-font setup).
- `dashboard/src/components/Layout.tsx` — single top-bar nav, sidebar + mobile bottom-nav removed.
- `dashboard/src/components/Navigation.tsx` — simplified to one `TopTab` component.
- `dashboard/src/components/KPICard.tsx` — flat restyle, simplified color prop.
- `dashboard/src/components/TrendChart.tsx`, `SessionTable.tsx` — ECharts retheme.
- `dashboard/src/components/ActivityTimeline.tsx`, `SystemLogs.tsx`, `DatePicker.tsx` — flat restyle.
- `dashboard/src/components/DashboardView.tsx` — gains accelerometer + min/max/avg stats folded in from Sensors.
- `dashboard/src/components/PowerView.tsx` — restyle only.
- `dashboard/src/components/SensorsView.tsx` — **deleted**.
- `dashboard/src/app/page.tsx` — `Tab` type narrowed, `SensorsView` references removed.
- `dashboard/package.json` — `recharts` dependency removed.
