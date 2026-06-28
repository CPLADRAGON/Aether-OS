# Design System — AETHER_OS Dashboard

## Product Context
- **What this is:** Real-time environmental monitoring dashboard for the AETHER_OS ESP32 room monitor
- **Who it's for:** Technical users monitoring room conditions, device health, and sync performance
- **Space/industry:** IoT / Smart Home / Environmental Monitoring
- **Project type:** Web dashboard (Next.js SPA)

## Aesthetic Direction
- **Direction:** Dark Cyber — command-center inspired, glassmorphism panels with neon accents
- **Decoration level:** Intentional — subtle backdrop blurs, glow effects, glass depth hierarchy
- **Mood:** Premium, technical, slightly dark. Like looking at a spaceship console. Data should feel alive.
- **Reference:** Stitch MCP-generated screens at `docs/pics/stitch-dashboard.html`

## Typography
- **Display/Hero:** Lexend (bold, condensed, techy) — for KPI values, section titles
- **Body:** Geist (clean, modern, highly readable at small sizes) — for labels, descriptions
- **UI/Labels:** Geist (same as body)
- **Data/Tables:** Geist (tabular-nums enabled) — for session tables, log timestamps
- **Code:** Geist Mono — for system log entries (terminal-style)
- **Loading:** Google Fonts CDN
- **Scale:**
  - Hero/KPI value: text-5xl (48px)
  - Section title: text-lg (18px)
  - Card label: text-[10px] uppercase tracking-widest
  - Body: text-xs (12px)
  - Tiny/Data: text-[10px]

## Color
- **Approach:** Balanced — primary + secondary + tertiary with semantic colors
- **Primary (Cyan):** `#00f3ff` — temperature data, active states, primary information
- **Secondary (Magenta):** `#cf5cff` — humidity data, secondary metrics
- **Tertiary (Lime):** `#a4f200` — light data, tertiary indicators
- **Neutrals:**
  - Background: `#050505` (deepest) / `#0a0a0a` (surface)
  - Surface: `#111417` (card surface)
  - Glass: `rgba(255, 255, 255, 0.03-0.05)` with `backdrop-filter: blur(12-40px)`
  - On-surface: `#e1e2e7` (white text)
  - Outline/muted: `#849495` / `rgba(255,255,255,0.4)`
- **Semantic:** 
  - Success: `#10b981` (emerald/green) — status, nominal indicators
  - Warning: `#f59e0b` (amber) — high session duration, warnings
  - Error: `#ef4444` (red) — errors, critical alerts
  - Info: `#00f3ff` (cyan) — informational
- **Dark mode only** — no light mode

## Spacing
- **Base unit:** 4px
- **Density:** Comfortable with data density
- **Scale:**
  - 2xs: 2px (progress bar height)
  - xs: 4px (panel padding)
  - sm: 8px (gap between elements)
  - md: 16px (card padding, grid gap)
  - lg: 24px (section gap)
  - xl: 32px (major section spacing)
  - 2xl: 48px

## Layout
- **Approach:** Grid-disciplined with hybrid content areas
- **Grid:** 
  - Desktop: 12-column grid, sidebar 64px/256px, max content 1600px
  - Tablet: 8-column grid, collapsed sidebar
  - Mobile: 4-column grid, bottom navigation, full-width cards
- **Max content width:** 1600px
- **Border radius:**
  - Default: 0.25rem (4px)
  - md: 0.5rem (8px) — cards
  - lg: 0.75rem (12px) — panels
  - xl: 1rem (16px) — major containers
  - full: 9999px — pills, badges

## Motion
- **Approach:** Intentional — subtle entrance animations, meaningful state transitions
- **Easing:** 
  - Enter: ease-out (spring for KPI values)
  - Exit: ease-in
  - Move: ease-in-out
- **Duration:**
  - Micro: 50-100ms (hover, active states)
  - Short: 150-250ms (panel transitions)
  - Medium: 300-500ms (page transitions)
  - Long: 600-1000ms (progress bars, pulse animations)

## Components

### Glass Panel
Base building block. Used for cards, sections, and containers.
```css
.glass-panel {
  background: rgba(255, 255, 255, 0.03);
  backdrop-filter: blur(12px);
  border: 1px solid rgba(255, 255, 255, 0.1);
}
.glass-panel-heavy {
  background: rgba(255, 255, 255, 0.05);
  backdrop-filter: blur(40px);
  border: 1px solid rgba(255, 255, 255, 0.15);
}
```

### KPI Card
Stat card with icon, value, unit, progress bar, and color-coded glow.
- Color accent via top border or text color
- Progress bar with matched glow shadow
- Animated value on change (framer-motion spring)

### Navigation
- Desktop: Fixed sidebar (hidden on mobile), persistent cyan active state
- Mobile: Bottom tab bar with 4 destinations + icons

### Status Badge
- Online: green dot + "ONLINE" text, pulse animation
- Connecting: amber dot + "CONNECTING"
- Offline: red dot + "OFFLINE"

## Decisions Log
| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-06-28 | Initial design system created | Created via Stitch MCP based on AETHER_OS dark cyber aesthetic |
| 2026-06-28 | Fonts: Lexend + Geist | Lexend for display/headlines (techy condensed), Geist for body (modern, readable at small sizes) |
| 2026-06-28 | Color: dark base with cyan/magenta/lime accents | Maintains existing AETHER brand while refining palette for better contrast |
| 2026-06-28 | Glassmorphism panels with backdrop blur | Creates depth hierarchy without heavy visual weight; consistent with existing dashboard |
