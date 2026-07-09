# Dashboard Minimal Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the dashboard's neon-cyberpunk visual theme with a minimal, calm Linear/Vercel-inspired look (flat cards, single indigo accent, Inter typeface, no blur/glow), simplify navigation to a single top bar, and fold the unused Sensors tab's useful content (accelerometer + min/max/avg stats) into the Dashboard tab.

**Architecture:** Restyle in place — every component keeps its existing props/data flow; only Tailwind classes, inline styles, and ECharts theme options change. Two structural changes ride along: nav simplification (`Layout.tsx`/`Navigation.tsx`) and the Sensors→Dashboard content merge (`DashboardView.tsx` gains stats, `SensorsView.tsx` is deleted, `page.tsx` drops the `sensors` tab).

**Tech Stack:** Next.js 16 (App Router), React 19, Tailwind CSS v4 (CSS-first `@theme inline` config), ECharts via `echarts-for-react`, Framer Motion, Supabase JS client.

**Spec:** `docs/superpowers/specs/2026-07-09-dashboard-minimal-redesign-design.md`

**Note on testing:** This project has no test runner configured (`npm run lint`/`build`/`dev`/`start` only, per `package.json`). Every task's verification step is `npm run build` (type-checks + builds) and, where noted, a manual `npm run dev` visual check — this is the correct equivalent for this codebase.

---

### Task 1: Theme tokens + Inter font

**Files:**
- Modify: `dashboard/src/app/globals.css` (full replacement)
- Modify: `dashboard/src/app/layout.tsx` (full replacement)

- [ ] **Step 1: Replace globals.css with the new flat theme**

Replace the entire contents of `dashboard/src/app/globals.css` with:

```css
@import "tailwindcss";

@theme inline {
  --color-background: #0d0d0f;
  --color-surface: #16161a;
  --color-surface-border: #1f1f23;
  --color-accent: #818cf8;
  --color-text-primary: #f4f4f5;
  --color-text-secondary: #a1a1aa;
  --color-text-tertiary: #6b7280;
  --color-status-online: #34d399;
  --color-status-warn: #fbbf24;
  --color-status-offline: #f87171;

  --font-sans: "Inter", -apple-system, sans-serif;
  --font-mono: "Geist Mono", monospace;
}

:root {
  --background: #0d0d0f;
  --foreground: #f4f4f5;
}

body {
  background-color: #0d0d0f;
  color: #f4f4f5;
  font-family: var(--font-sans);
}

.card {
  background: var(--color-surface);
  border: 1px solid var(--color-surface-border);
  border-radius: 8px;
}

/* Animations */
@keyframes pulse-dot {
  0%, 100% { opacity: 1; transform: scale(1); }
  50% { opacity: 0.6; transform: scale(1.15); }
}

.animate-pulse-dot {
  animation: pulse-dot 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
}

/* Custom scrollbar */
.custom-scrollbar::-webkit-scrollbar {
  width: 4px;
  height: 4px;
}
.custom-scrollbar::-webkit-scrollbar-track {
  background: rgba(255, 255, 255, 0.02);
}
.custom-scrollbar::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.15);
  border-radius: 10px;
}
```

This removes `.glass-panel`, `.glass-panel-heavy`, `.glow-cyan`, `.glow-magenta`, `.glow-lime`, `.active-glow`, `.progress-rainbow`, and the `--color-primary`/`--color-secondary`/`--color-tertiary`/`--color-outline`/`--font-headline`/`--font-body`/`--font-space` tokens. Every remaining reference to these is removed in later tasks (Tasks 2–8) — until those tasks run, `npm run build` will still succeed (Tailwind doesn't error on unknown utility-adjacent class names, they just render unstyled), but the app will look broken. That's expected for this task; don't try to fix every component yet.

- [ ] **Step 2: Replace layout.tsx with a single Inter font loader**

Replace the entire contents of `dashboard/src/app/layout.tsx` with:

```tsx
import type { Metadata } from "next";
import { Inter } from "next/font/google";
import "./globals.css";

const inter = Inter({
  variable: "--font-sans",
  subsets: ["latin"],
  display: "swap",
});

export const metadata: Metadata = {
  title: "Aether Dashboard",
  description: "Room monitoring dashboard",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en" className={`${inter.variable} h-full antialiased dark`}>
      <head>
        <link
          href="https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:wght,FILL@100..700,0..1&display=swap"
          rel="stylesheet"
        />
      </head>
      <body className="min-h-full flex flex-col font-sans">{children}</body>
    </html>
  );
}
```

Material Symbols stays (icons aren't changing, only typography/color/layout). Lexend and Geist font loaders are removed.

- [ ] **Step 3: Build to verify no compile errors**

Run: `cd dashboard; npm run build`
Expected: build succeeds (may show unstyled-looking output if you run `npm run dev` and look — that's expected until later tasks restyle each component; the build itself should not error).

- [ ] **Step 4: Commit**

```bash
git add dashboard/src/app/globals.css dashboard/src/app/layout.tsx
git commit -m "feat(dashboard): replace neon theme tokens with flat minimal palette"
```

---

### Task 2: Simplify Navigation component

**Files:**
- Modify: `dashboard/src/components/Navigation.tsx` (full replacement)

- [ ] **Step 1: Replace Navigation.tsx with a single TopTab component**

Replace the entire contents of `dashboard/src/components/Navigation.tsx` with:

```tsx
'use client';

interface TopTabProps {
  label: string;
  active?: boolean;
  onClick: () => void;
}

export function TopTab({ label, active = false, onClick }: TopTabProps) {
  return (
    <button
      onClick={onClick}
      className={`px-3 py-1.5 text-sm rounded-md transition-colors ${
        active
          ? 'bg-[#1f1f23] text-[#f4f4f5]'
          : 'text-[#a1a1aa] hover:text-[#f4f4f5] hover:bg-[#1f1f23]/60'
      }`}
    >
      {label}
    </button>
  );
}
```

This replaces `NavItem`, `MobileNavItem`, and `TimeToggle` (all three are only ever imported by `Layout.tsx`, which Task 3 rewrites to use `TopTab` instead; `TimeToggle` had zero importers anywhere in the codebase — confirmed dead code).

- [ ] **Step 2: Build to verify no compile errors**

Run: `cd dashboard; npm run build`
Expected: this will FAIL at this point, because `Layout.tsx` (not yet updated) still imports `NavItem`/`MobileNavItem` from this file. That's expected — proceed immediately to Task 3, which fixes it. Do not attempt to make Task 2 build green in isolation.

- [ ] **Step 3: Commit**

```bash
git add dashboard/src/components/Navigation.tsx
git commit -m "feat(dashboard): simplify Navigation to a single TopTab component"
```

---

### Task 3: Single top-bar Layout (remove sidebar + mobile bottom nav)

**Files:**
- Modify: `dashboard/src/components/Layout.tsx` (full replacement)

- [ ] **Step 1: Replace Layout.tsx**

Replace the entire contents of `dashboard/src/components/Layout.tsx` with:

```tsx
'use client';

import { TopTab } from '@/components/Navigation';

interface LayoutProps {
  realtimeStatus: 'connecting' | 'online' | 'offline';
  activeTab: 'dashboard' | 'power';
  onTabChange: (tab: 'dashboard' | 'power') => void;
  children: React.ReactNode;
}

export default function Layout({ realtimeStatus, activeTab, onTabChange, children }: LayoutProps) {
  const statusConfig = {
    online: {
      dot: 'bg-[#34d399] animate-pulse-dot',
      text: 'text-[#34d399]',
      label: 'Online',
    },
    connecting: {
      dot: 'bg-[#fbbf24]',
      text: 'text-[#fbbf24]',
      label: 'Connecting',
    },
    offline: {
      dot: 'bg-[#f87171]',
      text: 'text-[#f87171]',
      label: 'Offline',
    },
  };

  const status = statusConfig[realtimeStatus];

  return (
    <div className="min-h-screen bg-[#0d0d0f] text-[#f4f4f5]">
      <header className="sticky top-0 z-50 flex justify-between items-center px-6 py-3 w-full bg-[#0d0d0f] border-b border-[#1f1f23]">
        <div className="flex items-center gap-6">
          <span className="text-base font-semibold text-[#f4f4f5]">Aether</span>
          <nav className="flex items-center gap-1">
            <TopTab label="Dashboard" active={activeTab === 'dashboard'} onClick={() => onTabChange('dashboard')} />
            <TopTab label="Power" active={activeTab === 'power'} onClick={() => onTabChange('power')} />
          </nav>
        </div>
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <span className={`w-2 h-2 rounded-full ${status.dot}`} />
            <span className={`text-xs ${status.text}`}>{status.label}</span>
          </div>
          <div className="w-8 h-8 rounded-full bg-[#16161a] border border-[#1f1f23] flex items-center justify-center">
            <span className="material-symbols-outlined text-[#a1a1aa] text-sm">person</span>
          </div>
        </div>
      </header>

      <main className="flex-1 p-6">
        <div className="max-w-[1600px] mx-auto space-y-6">
          {children}
        </div>
      </main>
    </div>
  );
}
```

This removes the `<aside>` sidebar block, the mobile bottom `<nav>` block, and the `notifications` bell button (it had no `onClick` handler in the original — dead UI). The `Layout`'s `activeTab`/`onTabChange` prop types narrow from 3 tabs to 2 (`'dashboard' | 'power'`) — `page.tsx` is updated to match in Task 7.

- [ ] **Step 2: Build to verify no compile errors**

Run: `cd dashboard; npm run build`
Expected: this will still FAIL — `page.tsx` (not yet updated until Task 7) passes `activeTab`/`onTabChange` typed as including `'sensors'`, which no longer matches `Layout`'s narrowed prop type. That's expected. Continue to Task 4; Task 7 is what makes the whole build pass again.

- [ ] **Step 3: Commit**

```bash
git add dashboard/src/components/Layout.tsx
git commit -m "feat(dashboard): replace sidebar+mobile-nav with single top bar"
```

---

### Task 4: Flat KPICard restyle

**Files:**
- Modify: `dashboard/src/components/KPICard.tsx` (full replacement)

- [ ] **Step 1: Replace KPICard.tsx**

Replace the entire contents of `dashboard/src/components/KPICard.tsx` with:

```tsx
'use client';

import { motion } from 'framer-motion';

interface KPICardProps {
  icon: string;
  label: string;
  value: string;
  unit: string;
  status?: 'normal' | 'warn' | 'critical';
  progress: number;
  subLabel?: string;
}

const statusConfig = {
  normal: { text: 'text-[#f4f4f5]', bar: 'bg-[#818cf8]' },
  warn: { text: 'text-[#fbbf24]', bar: 'bg-[#fbbf24]' },
  critical: { text: 'text-[#f87171]', bar: 'bg-[#f87171]' },
};

export default function KPICard({ icon, label, value, unit, status = 'normal', progress, subLabel }: KPICardProps) {
  const cfg = statusConfig[status];

  return (
    <div className="card p-5">
      <div className="flex justify-between items-start mb-4">
        <div>
          <p className="text-xs text-[#6b7280]">{label}</p>
          <div className="flex items-baseline gap-1 mt-1">
            <motion.span
              key={value}
              initial={{ opacity: 0.4, y: -4 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ type: 'spring', stiffness: 300, damping: 20 }}
              className={`text-3xl font-semibold ${cfg.text} inline-block`}
            >
              {value}
            </motion.span>
            <span className="text-sm text-[#a1a1aa]">{unit}</span>
          </div>
        </div>
        <span className="material-symbols-outlined text-[#6b7280]">{icon}</span>
      </div>
      <div className="h-1 w-full bg-[#1f1f23] rounded-full overflow-hidden">
        <motion.div
          className={`h-full ${cfg.bar} rounded-full`}
          initial={{ width: 0 }}
          animate={{ width: `${Math.min(100, Math.max(0, progress))}%` }}
          transition={{ duration: 0.8, ease: 'easeOut' }}
        />
      </div>
      {subLabel && <p className="text-[11px] text-[#6b7280] mt-2">{subLabel}</p>}
    </div>
  );
}
```

The `color: 'cyan' | 'magenta' | 'lime' | 'amber' | 'white'` prop is replaced by `status?: 'normal' | 'warn' | 'critical'` (defaulting to `'normal'`). Call sites in `DashboardView.tsx`/`PowerView.tsx`/`SensorsView.tsx` (all updated in later tasks) currently pass `color="cyan"` etc — this WILL fail to build until those call sites are updated in Tasks 7–8.

- [ ] **Step 2: Build to verify (expected to still fail)**

Run: `cd dashboard; npm run build`
Expected: FAIL — `DashboardView.tsx`, `PowerView.tsx`, `SensorsView.tsx` still pass the old `color` prop. Expected at this stage; continue.

- [ ] **Step 3: Commit**

```bash
git add dashboard/src/components/KPICard.tsx
git commit -m "feat(dashboard): flatten KPICard, replace per-metric neon colors with status prop"
```

---

### Task 5: Retheme charts (TrendChart + SessionTable)

**Files:**
- Modify: `dashboard/src/components/TrendChart.tsx` (full replacement)
- Modify: `dashboard/src/components/SessionTable.tsx` (full replacement)

- [ ] **Step 1: Replace TrendChart.tsx**

Replace the entire contents of `dashboard/src/components/TrendChart.tsx` with:

```tsx
'use client';

/* eslint-disable @typescript-eslint/no-explicit-any */

import { useMemo } from 'react';
import ReactECharts from 'echarts-for-react';

interface Reading {
  id: number;
  created_at: string;
  temperature: number;
  humidity: number;
  ldr_value: number;
  accel_total: number;
  battery_v: number;
  trigger_source: string;
}

interface TrendChartProps {
  readings: Reading[];
  timeRange: { start: Date; end: Date };
  timeframe: 'day' | 'week' | 'month' | 'year';
}

export default function TrendChart({ readings, timeRange, timeframe }: TrendChartProps) {
  const chartOptions = useMemo(() => {
    const markAreaRanges: any[] = [];
    let startIdx = -1;
    for (let i = 0; i < readings.length; i++) {
      if (readings[i].ldr_value < 100 && startIdx === -1) {
        startIdx = i;
      } else if (readings[i].ldr_value >= 100 && startIdx !== -1) {
        markAreaRanges.push([
          { xAxis: new Date(readings[startIdx].created_at).getTime() },
          { xAxis: new Date(readings[i].created_at).getTime() },
        ]);
        startIdx = -1;
      }
    }
    if (startIdx !== -1 && readings.length > 0) {
      markAreaRanges.push([
        { xAxis: new Date(readings[startIdx].created_at).getTime() },
        { xAxis: new Date(readings[readings.length - 1].created_at).getTime() },
      ]);
    }

    return {
      tooltip: {
        trigger: 'axis',
        backgroundColor: '#16161a',
        borderColor: '#1f1f23',
        textStyle: { color: '#f4f4f5', fontSize: 11, fontFamily: 'Inter, sans-serif' },
        formatter: function (params: any) {
          const d = new Date(params[0].value[0]);
          let timeStr = '';
          if (timeframe === 'day') timeStr = d.toLocaleTimeString('en-SG', { timeZone: 'Asia/Singapore', hour: '2-digit', minute: '2-digit', hour12: false });
          else if (timeframe === 'week') timeStr = d.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', weekday: 'short', hour: '2-digit', minute: '2-digit', hour12: false });
          else timeStr = d.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit', hour12: false });

          let tooltipText = `<div style="font-size:10px;color:#6b7280;margin-bottom:4px;">${timeStr}</div>`;
          let t = 0, h = 0;
          params.forEach((param: any) => {
            const val = param.value[1];
            tooltipText += `<div style="display:flex;align-items:center;gap:6px;margin:2px 0;"><span style="display:inline-block;width:8px;height:8px;border-radius:50%;background-color:${param.color};"></span> <span style="font-weight:600">${param.seriesName}:</span> ${val}</div>`;
            if (param.seriesName === 'Temperature') t = val;
            if (param.seriesName === 'Humidity') h = val;
          });

          const hi = -8.78469475556 + 1.61139411 * t + 2.33854883889 * h - 0.14611605 * t * h - 0.012308094 * t * t - 0.0164248277778 * h * h + 0.002211732 * t * t * h + 0.00072546 * t * h * h - 0.000003582 * t * t * h * h;
          tooltipText += `<div style="margin-top:6px;padding-top:6px;border-top:1px solid #1f1f23;display:flex;align-items:center;gap:6px;"><span style="display:inline-block;width:8px;height:8px;border-radius:50%;background-color:#818cf8;"></span> <span style="font-weight:600">Apparent Temp:</span> ${hi.toFixed(1)} °C</div>`;
          return tooltipText;
        },
      },
      grid: { left: '3%', right: 80, bottom: '3%', top: '10%', containLabel: true },
      xAxis: {
        type: 'time',
        boundaryGap: false,
        axisLine: { show: false },
        axisTick: { show: false },
        axisLabel: {
          color: '#6b7280',
          fontSize: 10,
          fontFamily: 'Inter, sans-serif',
          formatter: (value: number) => {
            const d = new Date(value);
            if (timeframe === 'day') return d.toLocaleTimeString('en-SG', { timeZone: 'Asia/Singapore', hour: '2-digit', minute: '2-digit', hour12: false });
            if (timeframe === 'week') return d.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', weekday: 'short', hour: '2-digit', minute: '2-digit', hour12: false });
            if (timeframe === 'month') return d.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', day: 'numeric' });
            return d.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', year: 'numeric' });
          },
        },
        min: timeRange.start.getTime(),
        max: timeRange.end.getTime(),
      },
      yAxis: [
        {
          type: 'value',
          splitLine: { lineStyle: { type: 'dashed', color: '#1f1f23' } },
          axisLabel: { color: '#818cf8', fontSize: 10, fontFamily: 'Inter, sans-serif', formatter: (val: number) => `${val.toFixed(1)}°C` },
          min: 'dataMin',
          max: 'dataMax',
        },
        {
          type: 'value',
          position: 'right',
          splitLine: { show: false },
          axisLabel: { color: '#71717a', fontSize: 10, fontFamily: 'Inter, sans-serif', formatter: (val: number) => `${val.toFixed(1)}%` },
          min: 'dataMin',
          max: 'dataMax',
        },
        {
          type: 'value',
          position: 'right',
          offset: 50,
          splitLine: { show: false },
          axisLabel: { color: '#52525b', fontSize: 10, fontFamily: 'Inter, sans-serif', formatter: (val: number) => `${val.toFixed(0)}lx` },
          min: 0,
          max: 'dataMax',
        },
      ],
      series: [
        {
          name: 'Temperature',
          type: 'line',
          smooth: true,
          data: readings.map((r) => [new Date(r.created_at).getTime(), r.temperature]),
          itemStyle: { color: '#818cf8' },
          lineStyle: { width: 2.5 },
          showSymbol: false,
          markArea: {
            itemStyle: { color: 'rgba(255,255,255,0.04)' },
            data: markAreaRanges,
          },
        },
        {
          name: 'Humidity',
          type: 'line',
          smooth: true,
          yAxisIndex: 1,
          areaStyle: {
            color: {
              type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
              colorStops: [{ offset: 0, color: 'rgba(113,113,122,0.15)' }, { offset: 1, color: 'rgba(113,113,122,0)' }],
            },
          },
          data: readings.map((r) => [new Date(r.created_at).getTime(), r.humidity]),
          itemStyle: { color: '#71717a' },
          lineStyle: { width: 2 },
          showSymbol: false,
        },
        {
          name: 'Light',
          type: 'line',
          step: 'end',
          yAxisIndex: 2,
          data: readings.map((r) => [new Date(r.created_at).getTime(), r.ldr_value]),
          itemStyle: { color: '#52525b' },
          lineStyle: { width: 1, opacity: 0.7 },
          areaStyle: {
            color: {
              type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
              colorStops: [{ offset: 0, color: 'rgba(82,82,91,0.1)' }, { offset: 1, color: 'rgba(82,82,91,0)' }],
            },
          },
          showSymbol: false,
        },
      ],
    };
  }, [readings, timeRange, timeframe]);

  return (
    <ReactECharts option={chartOptions} style={{ height: '100%', width: '100%' }} opts={{ renderer: 'svg' }} />
  );
}
```

Changes: Temperature line stays the sole accent color (indigo `#818cf8`, was cyan). Humidity moves to `#71717a` (zinc-500) and Light to `#52525b` (zinc-600) — two muted neutrals instead of magenta/lime, per the spec. All `shadowBlur`/`shadowColor` glow effects on the temperature line are removed. Tooltip background/border go flat (`#16161a`/`#1f1f23`) instead of translucent-with-glow-border. All chart text fonts switch from `'Geist, sans-serif'` to `'Inter, sans-serif'`. The heat-index/apparent-temperature calculation and the dark-period `markArea` logic are untouched (data logic, not visual).

- [ ] **Step 2: Replace SessionTable.tsx**

Replace the entire contents of `dashboard/src/components/SessionTable.tsx` with:

```tsx
'use client';

import ReactECharts from 'echarts-for-react';

interface Session {
  duration: number;
  synced_at: string;
  start_time: number;
  boot_count: number;
  measure_count: number;
  sleep_interval?: number;
}

interface SessionTableProps {
  sessions: Session[];
}

export function SessionVolatilityChart({ sessions }: { sessions: Session[] }) {
  const data = sessions.slice(0, 20).reverse();
  return (
    <ReactECharts
      style={{ height: '280px' }}
      option={{
        backgroundColor: 'transparent',
        tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
        grid: { top: 20, left: 10, right: 10, bottom: 0, containLabel: true },
        xAxis: {
          type: 'category',
          data: data.map((_, i) => i + 1),
          axisLine: { show: false },
          axisLabel: { color: '#6b7280', fontSize: 10 },
        },
        yAxis: {
          type: 'value',
          splitLine: { lineStyle: { color: '#1f1f23' } },
          axisLabel: { color: '#6b7280', fontSize: 10 },
        },
        series: [
          {
            data: data.map((s) => ({
              value: s.duration,
              itemStyle: { color: s.duration > 25 ? '#fbbf24' : '#818cf8' },
            })),
            type: 'bar',
            barWidth: '60%',
            itemStyle: { borderRadius: [4, 4, 0, 0] },
          },
        ],
      }}
    />
  );
}

export function UptimeAccumulationChart({ sessions }: { sessions: Session[] }) {
  const data = sessions.slice(0, 20).reverse();
  return (
    <ReactECharts
      style={{ height: '280px' }}
      option={{
        backgroundColor: 'transparent',
        tooltip: { trigger: 'axis' },
        grid: { top: 20, left: 10, right: 10, bottom: 0, containLabel: true },
        xAxis: {
          type: 'category',
          data: data.map((s) => new Date(s.synced_at).toLocaleTimeString()),
          show: false,
        },
        yAxis: {
          type: 'value',
          splitLine: { show: false },
          axisLabel: { color: '#6b7280', fontSize: 10 },
        },
        series: [
          {
            data: data.reduce((acc: number[], s, i) => {
              const prev = i > 0 ? acc[i - 1] : 0;
              acc.push(prev + s.duration);
              return acc;
            }, []),
            type: 'line',
            smooth: true,
            symbol: 'none',
            lineStyle: { color: '#818cf8', width: 2.5 },
            areaStyle: {
              color: {
                type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                colorStops: [
                  { offset: 0, color: 'rgba(129,140,248,0.2)' },
                  { offset: 1, color: 'rgba(129,140,248,0)' },
                ],
              },
            },
          },
        ],
      }}
    />
  );
}

export default function SessionTable({ sessions }: SessionTableProps) {
  if (sessions.length === 0) {
    return (
      <div className="text-center py-8">
        <p className="text-[#6b7280] text-xs italic">No session data for this period.</p>
      </div>
    );
  }

  return (
    <div className="overflow-x-auto">
      <table className="w-full text-left border-collapse">
        <thead>
          <tr className="border-b border-[#1f1f23] text-xs text-[#6b7280]">
            <th className="py-3 px-2 font-medium">Timestamp (SGT)</th>
            <th className="py-3 px-2 font-medium">Duration</th>
            <th className="py-3 px-2 font-medium">Boot Index</th>
            <th className="py-3 px-2 font-medium">Sync Count</th>
            <th className="py-3 px-2 font-medium">Efficiency</th>
          </tr>
        </thead>
        <tbody className="text-xs font-mono">
          {sessions.map((s, idx) => (
            <tr key={idx} className="border-b border-[#1f1f23]/60 hover:bg-[#16161a] transition-colors">
              <td className="py-3 px-2 text-[#a1a1aa]">
                {new Date(s.synced_at).toLocaleString('en-SG', { timeZone: 'Asia/Singapore' })}
              </td>
              <td className="py-3 px-2 text-[#f4f4f5]">{s.duration}s</td>
              <td className="py-3 px-2 text-[#a1a1aa]">#{s.boot_count}</td>
              <td className="py-3 px-2 text-[#a1a1aa]">{s.measure_count}</td>
              <td className="py-3 px-2">
                <div className="w-24 bg-[#1f1f23] h-1 rounded-full overflow-hidden">
                  <div
                    className="h-full bg-[#34d399] rounded-full"
                    style={{ width: `${Math.min(100, (18 / s.duration) * 100)}%` }}
                  />
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
```

Note: the efficiency bar keeps `#34d399` (emerald) — that's a semantic "good" indicator, not a neon-decoration color, so it's kept per the spec's "muted semantic status colors stay" allowance. The volatility bar's amber-when->25s logic is also kept (semantic warning), just recolored from `#FBBF24`/`#00f3ff` to `#fbbf24`/`#818cf8`.

- [ ] **Step 3: Build to verify (still expected to fail on unrelated files)**

Run: `cd dashboard; npm run build`
Expected: FAIL — `DashboardView.tsx`/`PowerView.tsx`/`SensorsView.tsx` still reference old `KPICard` `color` prop from Task 4. This task's own files (`TrendChart.tsx`, `SessionTable.tsx`) introduce no new type errors on their own; the remaining failures are from files Task 7/8 will fix. Confirm the error output only mentions `color` prop mismatches, not anything in `TrendChart.tsx`/`SessionTable.tsx`.

- [ ] **Step 4: Commit**

```bash
git add dashboard/src/components/TrendChart.tsx dashboard/src/components/SessionTable.tsx
git commit -m "feat(dashboard): retheme ECharts colors to indigo + muted neutrals"
```

---

### Task 6: Flat restyle — ActivityTimeline, SystemLogs, DatePicker

**Files:**
- Modify: `dashboard/src/components/ActivityTimeline.tsx` (full replacement)
- Modify: `dashboard/src/components/SystemLogs.tsx` (full replacement)
- Modify: `dashboard/src/components/DatePicker.tsx` (full replacement)

- [ ] **Step 1: Replace ActivityTimeline.tsx**

Replace the entire contents of `dashboard/src/components/ActivityTimeline.tsx` with:

```tsx
'use client';

import { motion } from 'framer-motion';

interface PresenceEvent {
  time: string;
  label: 'User Out' | 'User Home';
}

interface ActivityTimelineProps {
  events: PresenceEvent[];
  formatSGTime: (dateStr: string) => string;
}

export default function ActivityTimeline({ events, formatSGTime }: ActivityTimelineProps) {
  if (events.length === 0) {
    return (
      <div className="flex-1 flex items-center justify-center">
        <p className="text-[#6b7280] text-xs italic">No recent activity shifts detected.</p>
      </div>
    );
  }

  return (
    <div className="space-y-6 relative before:absolute before:left-[11px] before:top-2 before:bottom-2 before:w-[2px] before:bg-[#1f1f23]">
      {events.map((evt, idx) => (
        <motion.div
          key={idx}
          initial={{ x: -10, opacity: 0 }}
          animate={{ x: 0, opacity: 1 }}
          transition={{ delay: idx * 0.1 }}
          className="flex gap-4 relative"
        >
          <div
            className={`w-6 h-6 rounded-full border-4 border-[#0d0d0f] z-10 flex items-center justify-center ${
              evt.label === 'User Home' ? 'bg-[#34d399]/20' : 'bg-[#f87171]/20'
            }`}
          >
            <div
              className={`w-1.5 h-1.5 rounded-full ${
                evt.label === 'User Home' ? 'bg-[#34d399]' : 'bg-[#f87171]'
              } ${evt.label === 'User Home' ? 'animate-pulse-dot' : ''}`}
            />
          </div>
          <div className="flex-1 pb-4 border-b border-[#1f1f23]">
            <div className="flex justify-between">
              <p className={`text-xs font-medium ${
                evt.label === 'User Home' ? 'text-[#34d399]' : 'text-[#f87171]'
              }`}>
                {evt.label}
              </p>
              <span className="text-[11px] text-[#6b7280] font-mono">{formatSGTime(evt.time)}</span>
            </div>
          </div>
        </motion.div>
      ))}
    </div>
  );
}
```

- [ ] **Step 2: Replace SystemLogs.tsx**

Replace the entire contents of `dashboard/src/components/SystemLogs.tsx` with:

```tsx
'use client';

interface DeviceLog {
  id: number;
  created_at: string;
  message: string;
  level: string;
}

interface SystemLogsProps {
  logs: DeviceLog[];
  formatSGTime: (dateStr: string) => string;
}

const levelBadge: Record<string, string> = {
  ERROR: 'bg-[#f87171]/15 text-[#f87171]',
  WARN: 'bg-[#fbbf24]/15 text-[#fbbf24]',
  INFO: 'bg-[#818cf8]/15 text-[#818cf8]',
  DEBUG: 'bg-[#3f3f46] text-[#a1a1aa]',
};

export default function SystemLogs({ logs, formatSGTime }: SystemLogsProps) {
  if (logs.length === 0) {
    return (
      <div className="bg-[#0d0d0f] rounded-lg border border-[#1f1f23] p-4 flex items-center justify-center min-h-[100px]">
        <p className="text-[#6b7280] text-xs italic">No system logs available.</p>
      </div>
    );
  }

  return (
    <div className="bg-[#0d0d0f] rounded-lg border border-[#1f1f23] p-3 text-[11px] space-y-1.5 overflow-y-auto custom-scrollbar flex-1 max-h-[300px]">
      {logs.map((log) => (
        <div key={log.id} className="flex gap-2 items-baseline">
          <span className="text-[#52525b] font-mono shrink-0">{formatSGTime(log.created_at)}</span>
          <span className={`px-1.5 py-0.5 rounded text-[9px] font-medium shrink-0 ${levelBadge[log.level] || 'bg-[#1f1f23] text-[#a1a1aa]'}`}>
            {log.level}
          </span>
          <span className="text-[#a1a1aa] font-mono">{log.message}</span>
        </div>
      ))}
    </div>
  );
}
```

Log level indicators move from bright colored bracketed text (`[ERROR]` in red) to small muted pill badges, per the spec.

- [ ] **Step 3: Replace DatePicker.tsx**

Replace the entire contents of `dashboard/src/components/DatePicker.tsx` with:

```tsx
import React, { useState } from 'react';

interface DatePickerProps {
  mode: 'day' | 'week' | 'month' | 'year';
  selectedDate: Date;
  onSelect: (date: Date) => void;
  minDate: Date | null;
  onClose: () => void;
}

export default function DatePicker({ mode, selectedDate, onSelect, minDate, onClose }: DatePickerProps) {
  const [viewDate, setViewDate] = useState(new Date(selectedDate));

  const daysInMonth = new Date(viewDate.getFullYear(), viewDate.getMonth() + 1, 0).getDate();
  const firstDayOfMonth = new Date(viewDate.getFullYear(), viewDate.getMonth(), 1).getDay();
  const startOffset = firstDayOfMonth === 0 ? 6 : firstDayOfMonth - 1;

  const today = new Date();
  today.setHours(0, 0, 0, 0);

  const safeMinDate = minDate ? new Date(minDate) : null;
  if (safeMinDate) safeMinDate.setHours(0, 0, 0, 0);

  const isDateDisabled = (date: Date) => {
    if (date > today) return true;
    if (safeMinDate && date < safeMinDate) return true;
    return false;
  };

  if (mode === 'day' || mode === 'week') {
    const days = Array.from({ length: 42 }, (_, i) => {
      const dayNum = i - startOffset + 1;
      const date = new Date(viewDate.getFullYear(), viewDate.getMonth(), dayNum);
      date.setHours(0, 0, 0, 0);
      return { date, isCurrentMonth: dayNum > 0 && dayNum <= daysInMonth };
    });

    const shiftMonth = (dir: number) => {
      setViewDate((prev) => new Date(prev.getFullYear(), prev.getMonth() + dir, 1));
    };

    return (
      <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 z-50 p-4 bg-[#16161a] border border-[#1f1f23] rounded-xl shadow-xl w-72 flex flex-col gap-4">
        <div className="flex justify-between items-center text-[#f4f4f5]">
          <button onClick={() => shiftMonth(-1)} className="p-1 hover:bg-[#1f1f23] rounded"><span className="material-symbols-outlined text-[16px]">chevron_left</span></button>
          <div className="font-medium text-sm">{viewDate.toLocaleDateString('en-SG', { month: 'long', year: 'numeric' })}</div>
          <button onClick={() => shiftMonth(1)} className="p-1 hover:bg-[#1f1f23] rounded"><span className="material-symbols-outlined text-[16px]">chevron_right</span></button>
        </div>
        <div className="grid grid-cols-7 gap-1 text-center text-[10px] text-[#6b7280] mb-2">
          {['M', 'T', 'W', 'T', 'F', 'S', 'S'].map((d, i) => <div key={i}>{d}</div>)}
        </div>
        <div className="grid grid-cols-7 gap-y-1">
          {mode === 'day' ? (
            days.map((d, i) => {
              const disabled = isDateDisabled(d.date);
              const isSelected = d.date.getTime() === new Date(selectedDate).setHours(0, 0, 0, 0);
              return (
                <button
                  key={i}
                  disabled={disabled}
                  onClick={() => { onSelect(d.date); onClose(); }}
                  className={`h-8 w-8 mx-auto flex items-center justify-center rounded text-xs transition-colors
                    ${!d.isCurrentMonth ? 'opacity-20' : ''}
                    ${disabled ? 'opacity-20 cursor-not-allowed' : 'hover:bg-[#818cf8]/20'}
                    ${isSelected ? 'bg-[#818cf8] text-[#0d0d0f] font-semibold hover:bg-[#818cf8]' : 'text-[#f4f4f5]'}`}
                >
                  {d.date.getDate()}
                </button>
              );
            })
          ) : (
            Array.from({ length: 6 }).map((_, rowIdx) => {
              const weekDays = days.slice(rowIdx * 7, rowIdx * 7 + 7);
              const selectedStart = new Date(selectedDate);
              let sDay = selectedStart.getDay();
              if (sDay === 0) sDay = 7;
              selectedStart.setDate(selectedStart.getDate() - sDay + 1);
              selectedStart.setHours(0, 0, 0, 0);
              const selectedEnd = new Date(selectedStart);
              selectedEnd.setDate(selectedStart.getDate() + 6);

              const isSelected = weekDays.some((d) => d.date >= selectedStart && d.date <= selectedEnd);
              const disabled = weekDays.every((d) => isDateDisabled(d.date));

              return (
                <div
                  key={rowIdx}
                  className={`col-span-7 grid grid-cols-7 rounded transition-colors cursor-pointer
                    ${disabled ? 'opacity-30 cursor-not-allowed' : 'hover:bg-[#818cf8]/10'}
                    ${isSelected ? 'bg-[#818cf8]/20 ring-1 ring-[#818cf8]/50' : ''}`}
                  onClick={() => { if (!disabled) { onSelect(weekDays[0].date); onClose(); } }}
                >
                  {weekDays.map((d, i) => (
                    <div key={i} className={`h-8 flex items-center justify-center text-xs ${!d.isCurrentMonth ? 'opacity-30' : ''} ${isSelected ? 'text-[#818cf8] font-semibold' : 'text-[#f4f4f5]'}`}>
                      {d.date.getDate()}
                    </div>
                  ))}
                </div>
              );
            })
          )}
        </div>
      </div>
    );
  }

  if (mode === 'month') {
    const shiftYear = (dir: number) => {
      setViewDate((prev) => new Date(prev.getFullYear() + dir, 0, 1));
    };

    const months = Array.from({ length: 12 }, (_, i) => new Date(viewDate.getFullYear(), i, 1));

    return (
      <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 z-50 p-4 bg-[#16161a] border border-[#1f1f23] rounded-xl shadow-xl w-64 flex flex-col gap-4">
        <div className="flex justify-between items-center text-[#f4f4f5]">
          <button onClick={() => shiftYear(-1)} className="p-1 hover:bg-[#1f1f23] rounded"><span className="material-symbols-outlined text-[16px]">chevron_left</span></button>
          <div className="font-medium text-sm">{viewDate.getFullYear()}</div>
          <button onClick={() => shiftYear(1)} className="p-1 hover:bg-[#1f1f23] rounded"><span className="material-symbols-outlined text-[16px]">chevron_right</span></button>
        </div>
        <div className="grid grid-cols-3 gap-2">
          {months.map((d, i) => {
            const isSelected = d.getFullYear() === selectedDate.getFullYear() && d.getMonth() === selectedDate.getMonth();
            const lastDayOfMonth = new Date(d.getFullYear(), d.getMonth() + 1, 0);
            const disabled = isDateDisabled(d) && isDateDisabled(lastDayOfMonth);
            return (
              <button
                key={i}
                disabled={disabled}
                onClick={() => { onSelect(d); onClose(); }}
                className={`py-2 rounded text-xs transition-colors
                  ${disabled ? 'opacity-20 cursor-not-allowed' : 'hover:bg-[#818cf8]/20'}
                  ${isSelected ? 'bg-[#818cf8] text-[#0d0d0f] font-semibold' : 'text-[#f4f4f5] bg-[#1f1f23]'}`}
              >
                {d.toLocaleDateString('en-SG', { month: 'short' })}
              </button>
            );
          })}
        </div>
      </div>
    );
  }

  if (mode === 'year') {
    const startDecade = Math.floor(viewDate.getFullYear() / 10) * 10;
    const shiftDecade = (dir: number) => {
      setViewDate((prev) => new Date(prev.getFullYear() + dir * 10, 0, 1));
    };

    const years = Array.from({ length: 12 }, (_, i) => new Date(startDecade - 1 + i, 0, 1));

    return (
      <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 z-50 p-4 bg-[#16161a] border border-[#1f1f23] rounded-xl shadow-xl w-64 flex flex-col gap-4">
        <div className="flex justify-between items-center text-[#f4f4f5]">
          <button onClick={() => shiftDecade(-1)} className="p-1 hover:bg-[#1f1f23] rounded"><span className="material-symbols-outlined text-[16px]">chevron_left</span></button>
          <div className="font-medium text-sm">{startDecade} – {startDecade + 9}</div>
          <button onClick={() => shiftDecade(1)} className="p-1 hover:bg-[#1f1f23] rounded"><span className="material-symbols-outlined text-[16px]">chevron_right</span></button>
        </div>
        <div className="grid grid-cols-3 gap-2">
          {years.map((d, i) => {
            const isSelected = d.getFullYear() === selectedDate.getFullYear();
            const lastDayOfYear = new Date(d.getFullYear(), 11, 31);
            const disabled = isDateDisabled(d) && isDateDisabled(lastDayOfYear);
            const isOutDecade = i === 0 || i === 11;
            return (
              <button
                key={i}
                disabled={disabled}
                onClick={() => { onSelect(d); onClose(); }}
                className={`py-2 rounded text-xs transition-colors
                  ${isOutDecade ? 'opacity-30' : ''}
                  ${disabled ? 'opacity-20 cursor-not-allowed' : 'hover:bg-[#818cf8]/20'}
                  ${isSelected ? 'bg-[#818cf8] text-[#0d0d0f] font-semibold' : 'text-[#f4f4f5] bg-[#1f1f23]'}`}
              >
                {d.getFullYear()}
              </button>
            );
          })}
        </div>
      </div>
    );
  }

  return null;
}
```

Removes `font-space`, the `text-outline` reference (undefined after Task 1's theme removal — this was the bug this rewrite fixes), `backdrop-blur-xl`, and all `cyan-*`/`slate-950` Tailwind palette colors in favor of the flat theme's arbitrary hex values.

- [ ] **Step 4: Build to verify (still expected to fail on unrelated files)**

Run: `cd dashboard; npm run build`
Expected: FAIL — same remaining `KPICard` `color`-prop mismatches in `DashboardView.tsx`/`PowerView.tsx`/`SensorsView.tsx` from Task 4. Confirm no NEW errors from `ActivityTimeline.tsx`/`SystemLogs.tsx`/`DatePicker.tsx`.

- [ ] **Step 5: Commit**

```bash
git add dashboard/src/components/ActivityTimeline.tsx dashboard/src/components/SystemLogs.tsx dashboard/src/components/DatePicker.tsx
git commit -m "feat(dashboard): flat restyle for ActivityTimeline, SystemLogs, DatePicker"
```

---

### Task 7: Dashboard restyle + fold in Sensors content + delete SensorsView + update page.tsx

**Files:**
- Modify: `dashboard/src/components/DashboardView.tsx` (full replacement)
- Delete: `dashboard/src/components/SensorsView.tsx`
- Modify: `dashboard/src/app/page.tsx` (full replacement)

This is the task that finally makes `npm run build` pass again — it's the last file touching `KPICard`'s `color`→`status` prop rename, the last file importing the now-deleted `SensorsView`, and the file that narrows the `Tab` type to match `Layout.tsx`'s new 2-tab prop type from Task 3.

- [ ] **Step 1: Replace DashboardView.tsx**

Replace the entire contents of `dashboard/src/components/DashboardView.tsx` with:

```tsx
'use client';

import { motion } from 'framer-motion';
import { useMemo } from 'react';
import KPICard from '@/components/KPICard';
import TrendChart from '@/components/TrendChart';
import ActivityTimeline from '@/components/ActivityTimeline';
import SystemLogs from '@/components/SystemLogs';

interface Reading {
  id: number;
  created_at: string;
  temperature: number;
  humidity: number;
  ldr_value: number;
  accel_total: number;
  battery_v: number;
  trigger_source: string;
}

interface DeviceLog {
  id: number;
  created_at: string;
  message: string;
  level: string;
}

interface PresenceEvent {
  time: string;
  label: 'User Out' | 'User Home';
}

interface DashboardViewProps {
  readings: Reading[];
  latest: Reading | null;
  logs: DeviceLog[];
  timeframe: 'day' | 'week' | 'month' | 'year';
  timeRange: { start: Date; end: Date };
  loading: boolean;
  presenceEvents: PresenceEvent[];
  formatSGTime: (dateStr: string) => string;
  comfortScore: number;
  displayData: { temp: number; hum: number; ldr: number; label: string };
  periodNavigation: React.ReactNode;
}

function StatChip({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex flex-col gap-0.5 px-3 py-2 rounded-md bg-[#0d0d0f] border border-[#1f1f23]">
      <span className="text-[10px] text-[#6b7280]">{label}</span>
      <span className="text-xs font-mono text-[#a1a1aa]">{value}</span>
    </div>
  );
}

export default function DashboardView({
  readings,
  latest,
  logs,
  timeframe,
  timeRange,
  loading,
  presenceEvents,
  formatSGTime,
  comfortScore,
  displayData,
  periodNavigation,
}: DashboardViewProps) {
  const stats = useMemo(() => {
    if (readings.length === 0) return null;
    const avg = (arr: number[]) => arr.reduce((a, b) => a + b, 0) / arr.length;
    const temps = readings.map((r) => r.temperature);
    const hums = readings.map((r) => r.humidity);
    const ldrs = readings.map((r) => r.ldr_value);
    const accels = readings.map((r) => r.accel_total);
    return {
      temp: { min: Math.min(...temps), max: Math.max(...temps), avg: avg(temps) },
      hum: { min: Math.min(...hums), max: Math.max(...hums), avg: avg(hums) },
      ldr: { min: Math.min(...ldrs), max: Math.max(...ldrs), avg: avg(ldrs) },
      accel: { min: Math.min(...accels), max: Math.max(...accels), avg: avg(accels) },
    };
  }, [readings]);

  if (loading && readings.length === 0) {
    return (
      <div className="flex items-center justify-center min-h-[60vh]">
        <div className="flex flex-col items-center gap-4">
          <div className="w-10 h-10 border-2 border-[#818cf8] border-t-transparent rounded-full animate-spin" />
          <p className="text-[#a1a1aa] text-sm">Loading…</p>
        </div>
      </div>
    );
  }

  return (
    <>
      <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-4 gap-6">
        <KPICard
          icon="thermostat"
          label="Temperature"
          value={displayData.temp.toFixed(1)}
          unit="°C"
          progress={(displayData.temp / 50) * 100}
          subLabel={displayData.label}
        />
        <KPICard
          icon="humidity_percentage"
          label="Humidity"
          value={displayData.hum.toFixed(1)}
          unit="%"
          progress={displayData.hum}
          subLabel={displayData.label}
        />
        <KPICard
          icon="light_mode"
          label="Light Intensity"
          value={displayData.ldr.toFixed(0)}
          unit="lux"
          progress={(displayData.ldr / 4095) * 100}
          subLabel={displayData.label}
        />

        <div className="card p-5 flex flex-col items-center justify-center gap-2">
          <p className="text-xs text-[#6b7280]">Comfort Index</p>
          <motion.div
            key={comfortScore}
            initial={{ opacity: 0.5 }}
            animate={{ opacity: 1 }}
            transition={{ type: 'spring', stiffness: 200, damping: 15 }}
            className="flex items-baseline gap-1"
          >
            <span className="text-4xl font-semibold text-[#f4f4f5]">{comfortScore}</span>
            <span className="text-sm text-[#6b7280]">/100</span>
          </motion.div>
          <div className="w-full bg-[#1f1f23] h-1 rounded-full mt-3 overflow-hidden">
            <motion.div
              className="h-full bg-[#818cf8] rounded-full"
              initial={{ width: 0 }}
              animate={{ width: `${comfortScore}%` }}
              transition={{ duration: 0.8 }}
            />
          </div>
        </div>
      </div>

      {latest && stats && (
        <div className="card p-5">
          <p className="text-xs text-[#6b7280] mb-3">Sensor Details</p>
          <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-5 gap-2">
            <StatChip label="Temp Min" value={`${stats.temp.min.toFixed(1)}°C`} />
            <StatChip label="Temp Max" value={`${stats.temp.max.toFixed(1)}°C`} />
            <StatChip label="Temp Avg" value={`${stats.temp.avg.toFixed(1)}°C`} />
            <StatChip label="Humidity Avg" value={`${stats.hum.avg.toFixed(1)}%`} />
            <StatChip label="Light Avg" value={`${stats.ldr.avg.toFixed(0)} lx`} />
            <StatChip label="Accelerometer" value={`${latest.accel_total.toFixed(2)} m/s²`} />
            <StatChip label="Accel Min" value={`${stats.accel.min.toFixed(2)} m/s²`} />
            <StatChip label="Accel Max" value={`${stats.accel.max.toFixed(2)} m/s²`} />
            <StatChip label="Accel Avg" value={`${stats.accel.avg.toFixed(2)} m/s²`} />
          </div>
        </div>
      )}

      <div className="grid grid-cols-1 xl:grid-cols-10 gap-6">
        <div className="xl:col-span-7">
          <div className="card p-6 min-h-[500px] flex flex-col">
            <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-4 mb-6">
              <h2 className="text-sm font-medium text-[#a1a1aa]">Trend Analysis</h2>
              <div className="flex items-center gap-2">{periodNavigation}</div>
            </div>
            <div className="flex-1 min-h-[350px]">
              <TrendChart readings={readings} timeRange={timeRange} timeframe={timeframe} />
            </div>
            <div className="flex gap-6 mt-6 border-t border-[#1f1f23] pt-4 overflow-x-auto">
              <LegendItem color="bg-[#818cf8]" label="Temperature (°C)" />
              <LegendItem color="bg-[#71717a]" label="Humidity (%)" />
              <LegendItem color="bg-[#52525b]" label="Light (Lux)" />
            </div>
          </div>
        </div>

        <div className="xl:col-span-3 flex flex-col gap-6">
          <div className="card p-6 min-h-[200px]">
            <h3 className="text-sm font-medium text-[#a1a1aa] mb-6">Activity</h3>
            <ActivityTimeline events={presenceEvents} formatSGTime={formatSGTime} />
          </div>

          <div className="card p-6 flex flex-col min-h-[250px]">
            <h3 className="text-sm font-medium text-[#a1a1aa] mb-4">System Logs</h3>
            <SystemLogs logs={logs} formatSGTime={formatSGTime} />
          </div>
        </div>
      </div>
    </>
  );
}

function LegendItem({ color, label }: { color: string; label: string }) {
  return (
    <div className="flex items-center gap-2 whitespace-nowrap">
      <span className={`w-3 h-1 rounded ${color}`} />
      <span className="text-[11px] text-[#6b7280]">{label}</span>
    </div>
  );
}
```

Changes: gains a `latest: Reading | null` prop; computes the same min/max/avg `stats` `useMemo` that `SensorsView.tsx` used to (minus `battery_v`, per the spec's Goal 4); renders a new "Sensor Details" card (guarded on `latest && stats` being present, matching `SensorsView`'s original null-guard); `KPICard` calls drop the `color` prop entirely (defaults to `'normal'` — none of these four values need a warn/critical state); Comfort Index card and Trend/Activity/Logs cards all use the flat `.card` utility instead of `glass-panel-heavy`; the colored `w-1 h-6 bg-[...]` accent ticks before each section heading are removed (plain `text-sm font-medium text-[#a1a1aa]` headings instead); legend colors match `TrendChart.tsx`'s new indigo/zinc-500/zinc-600 palette.

- [ ] **Step 2: Delete SensorsView.tsx**

```bash
rm dashboard/src/components/SensorsView.tsx
```

- [ ] **Step 3: Replace page.tsx**

Replace the entire contents of `dashboard/src/app/page.tsx` with:

```tsx
'use client';

import { useCallback, useEffect, useState, useMemo } from 'react';
import DatePicker from '@/components/DatePicker';
import Layout from '@/components/Layout';
import DashboardView from '@/components/DashboardView';
import PowerView from '@/components/PowerView';
import { supabase } from '@/lib/supabase';

// Polyfill for crypto.randomUUID (Required for non-secure IP access)
if (typeof window !== 'undefined' && !window.crypto.randomUUID) {
  // @ts-expect-error - polyfill for environments without crypto.randomUUID
  window.crypto.randomUUID = () => {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
      const r = (Math.random() * 16) | 0;
      const v = c === 'x' ? r : (r & 0x3) | 0x8;
      return v.toString(16);
    });
  };
}

interface Reading {
  id: number;
  created_at: string;
  temperature: number;
  humidity: number;
  ldr_value: number;
  accel_total: number;
  battery_v: number;
  trigger_source: string;
}

interface DeviceLog {
  id: number;
  created_at: string;
  message: string;
  level: string;
}

interface Session {
  duration: number;
  synced_at: string;
  start_time: number;
  boot_count: number;
  measure_count: number;
  sleep_interval?: number;
}

type Timeframe = 'day' | 'week' | 'month' | 'year';
type Tab = 'dashboard' | 'power';

export default function Dashboard() {
  // State
  const [readings, setReadings] = useState<Reading[]>([]);
  const [latest, setLatest] = useState<Reading | null>(null);
  const [logs, setLogs] = useState<DeviceLog[]>([]);
  const [sessions, setSessions] = useState<Session[]>([]);
  const [loading, setLoading] = useState(true);
  const [timeframe, setTimeframe] = useState<Timeframe>('day');
  const [referenceDate, setReferenceDate] = useState<Date>(new Date());
  const [isDatePickerOpen, setIsDatePickerOpen] = useState(false);
  const [oldestDate, setOldestDate] = useState<Date | null>(null);
  const [realtimeStatus, setRealtimeStatus] = useState<'connecting' | 'online' | 'offline'>('connecting');
  const [activeTab, setActiveTab] = useState<Tab>('dashboard');

  // Time range calculation
  const timeRange = useMemo(() => {
    const start = new Date(referenceDate);
    const end = new Date(referenceDate);
    start.setHours(0, 0, 0, 0);
    end.setHours(23, 59, 59, 999);
    if (timeframe === 'week') {
      let currentDay = start.getDay();
      if (currentDay === 0) currentDay = 7;
      start.setDate(start.getDate() - currentDay + 1);
      end.setDate(start.getDate() + 6);
    } else if (timeframe === 'month') {
      start.setDate(1);
      end.setFullYear(start.getFullYear(), start.getMonth() + 1, 0);
    } else if (timeframe === 'year') {
      start.setMonth(0, 1);
      end.setFullYear(start.getFullYear(), 11, 31);
    }
    return { start, end };
  }, [timeframe, referenceDate]);

  const isCurrentPeriod = useMemo(() => {
    const now = new Date();
    return now >= timeRange.start && now <= timeRange.end;
  }, [timeRange]);

  const canGoForward = useMemo(() => {
    const nextStart = new Date(timeRange.end);
    nextStart.setMilliseconds(nextStart.getMilliseconds() + 1);
    return nextStart <= new Date();
  }, [timeRange]);

  const canGoBackward = useMemo(() => {
    if (!oldestDate) return true;
    return timeRange.start > oldestDate;
  }, [timeRange, oldestDate]);

  const shiftPeriod = (direction: -1 | 1) => {
    setReferenceDate((prev) => {
      const d = new Date(prev);
      if (timeframe === 'day') d.setDate(d.getDate() + direction);
      else if (timeframe === 'week') d.setDate(d.getDate() + direction * 7);
      else if (timeframe === 'month') d.setMonth(d.getMonth() + direction);
      else if (timeframe === 'year') d.setFullYear(d.getFullYear() + direction);
      return d;
    });
  };

  // Data fetching effect
  useEffect(() => {
    const doFetch = async () => {
      setLoading(true);
      const { data: readingsData } = await supabase
        .from('room_readings')
        .select('*')
        .gte('created_at', timeRange.start.toISOString())
        .lte('created_at', timeRange.end.toISOString())
        .order('created_at', { ascending: true })
        .limit(1000);
      if (readingsData) {
        setReadings(readingsData);
        if (readingsData.length > 0) setLatest(readingsData[readingsData.length - 1]);
      }
      setLoading(false);

      const { data: logsData } = await supabase
        .from('device_logs')
        .select('*')
        .gte('created_at', timeRange.start.toISOString())
        .lte('created_at', timeRange.end.toISOString())
        .order('created_at', { ascending: false })
        .limit(200);
      if (logsData) setLogs(logsData);

      if (activeTab === 'power') {
        const { data: sessionsData } = await supabase
          .from('device_sessions')
          .select('*')
          .order('synced_at', { ascending: false })
          .limit(5000);
        if (sessionsData) setSessions(sessionsData);
      }
    };
    doFetch();
  }, [timeRange, activeTab]);

  // Init: oldest date + realtime subscriptions
  useEffect(() => {
    async function fetchOldestDate() {
      const { data } = await supabase
        .from('room_readings')
        .select('created_at')
        .order('created_at', { ascending: true })
        .limit(1);
      if (data && data.length > 0) setOldestDate(new Date(data[0].created_at));
    }
    fetchOldestDate();

    const channel = supabase
      .channel('live_updates')
      .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'room_readings' }, (payload) => {
        const newReading = payload.new as Reading;
        setReadings((prev) => [...prev.slice(-999), newReading]);
        setLatest(newReading);
      })
      .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'device_logs' }, (payload) => {
        setLogs((prev) => [payload.new as DeviceLog, ...prev].slice(0, 50));
      })
      .subscribe((status) => {
        if (status === 'SUBSCRIBED') setRealtimeStatus('online');
        else if (status === 'CLOSED' || status === 'CHANNEL_ERROR') setRealtimeStatus('offline');
      });

    return () => {
      supabase.removeChannel(channel);
    };
  }, []);

  // Derived data
  const formatSGTime = useCallback((dateStr: string) => {
    return new Date(dateStr).toLocaleString('en-SG', {
      timeZone: 'Asia/Singapore',
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });
  }, []);

  const averages = useMemo(() => {
    if (readings.length === 0) return { temp: 0, hum: 0, ldr: 0 };
    const sum = readings.reduce(
      (acc, r) => ({
        temp: acc.temp + r.temperature,
        hum: acc.hum + r.humidity,
        ldr: acc.ldr + r.ldr_value,
      }),
      { temp: 0, hum: 0, ldr: 0 }
    );
    return {
      temp: sum.temp / readings.length,
      hum: sum.hum / readings.length,
      ldr: sum.ldr / readings.length,
    };
  }, [readings]);

  const displayData =
    timeframe === 'day'
      ? {
          temp: latest?.temperature || 0,
          hum: latest?.humidity || 0,
          ldr: latest?.ldr_value || 0,
          label: 'Latest sync',
        }
      : {
          temp: averages.temp,
          hum: averages.hum,
          ldr: averages.ldr,
          label: `Period average (${timeframe})`,
        };

  const comfortScore = useMemo(() => {
    if (!latest) return 0;
    const t = latest.temperature;
    const h = latest.humidity;
    let tScore = 100 - Math.abs(t - 24) * 5;
    let hScore = 100 - Math.abs(h - 50) * 1.5;
    tScore = Math.max(0, Math.min(100, tScore));
    hScore = Math.max(0, Math.min(100, hScore));
    return Math.round((tScore + hScore) / 2);
  }, [latest]);

  const presenceEvents = useMemo(() => {
    const events: { time: string; label: 'User Out' | 'User Home' }[] = [];
    if (readings.length < 2) return events;
    const thirtyMinsAgo = new Date();
    thirtyMinsAgo.setMinutes(thirtyMinsAgo.getMinutes() - 30);
    const recent = readings.filter((r) => new Date(r.created_at) >= thirtyMinsAgo);
    if (recent.length < 5) return events;
    let currentState = 'Home';
    for (let i = 5; i < recent.length; i++) {
      const past = recent[i - 5];
      const current = recent[i];
      const ldrChange = (current.ldr_value - past.ldr_value) / Math.max(1, past.ldr_value);
      const humChange = current.humidity - past.humidity;
      if (ldrChange <= -0.5 && humChange <= -0.5 && currentState !== 'Out') {
        events.push({ time: current.created_at, label: 'User Out' });
        currentState = 'Out';
      } else if (ldrChange >= 0.5 && humChange >= 0.5 && currentState !== 'Home') {
        events.push({ time: current.created_at, label: 'User Home' });
        currentState = 'Home';
      }
    }
    return events.reverse().slice(0, 5);
  }, [readings]);

  const lastUpdateStr = latest
    ? new Date(latest.created_at).toLocaleTimeString([], {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false,
      })
    : '--:--:--';

  const filteredSessions = useMemo(() => {
    return sessions.filter((s) => {
      const d = new Date(s.synced_at);
      return d >= timeRange.start && d <= timeRange.end;
    });
  }, [sessions, timeRange]);

  // Period navigation UI
  const periodLabel = useMemo(() => {
    const start = timeRange.start;
    if (timeframe === 'day') {
      if (isCurrentPeriod) return 'Today';
      return start.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', day: 'numeric', year: 'numeric' });
    } else if (timeframe === 'week') {
      const end = timeRange.end;
      if (isCurrentPeriod) return 'This week';
      return `${start.toLocaleDateString('en-SG', { month: 'short', day: 'numeric' })} – ${end.toLocaleDateString('en-SG', { month: 'short', day: 'numeric', year: 'numeric' })}`;
    } else if (timeframe === 'month') {
      if (isCurrentPeriod) return 'This month';
      return start.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', year: 'numeric' });
    } else {
      if (isCurrentPeriod) return 'This year';
      return start.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', year: 'numeric' });
    }
  }, [timeRange, timeframe, isCurrentPeriod]);

  const periodNavigation = (
    <div className="flex items-center gap-2">
      <div className="flex bg-[#16161a] p-1 rounded-lg border border-[#1f1f23] gap-1">
        <button
          onClick={() => shiftPeriod(-1)}
          disabled={!canGoBackward}
          className={`px-2 py-1 flex items-center rounded transition-colors ${
            !canGoBackward ? 'text-[#3f3f46] cursor-not-allowed' : 'text-[#a1a1aa] hover:bg-[#1f1f23]'
          }`}
        >
          <span className="material-symbols-outlined text-[16px]">chevron_left</span>
        </button>
        <div className="relative flex items-center justify-center min-w-[100px] md:min-w-[130px]">
          <button
            onClick={() => setIsDatePickerOpen(!isDatePickerOpen)}
            className="flex items-center gap-1 text-[#a1a1aa] hover:text-[#f4f4f5] transition-colors"
          >
            <span className="material-symbols-outlined text-[14px]">calendar_today</span>
            <span className="text-xs">{periodLabel}</span>
          </button>
          {isDatePickerOpen && (
            <DatePicker
              mode={timeframe}
              selectedDate={referenceDate}
              minDate={oldestDate}
              onSelect={(d) => {
                setReferenceDate(d);
                setIsDatePickerOpen(false);
              }}
              onClose={() => setIsDatePickerOpen(false)}
            />
          )}
        </div>
        <button
          onClick={() => shiftPeriod(1)}
          disabled={!canGoForward}
          className={`px-2 py-1 flex items-center rounded transition-colors ${
            !canGoForward ? 'text-[#3f3f46] cursor-not-allowed' : 'text-[#a1a1aa] hover:bg-[#1f1f23]'
          }`}
        >
          <span className="material-symbols-outlined text-[16px]">chevron_right</span>
        </button>
      </div>
      <div className="flex bg-[#16161a] p-1 rounded-lg border border-[#1f1f23] gap-1">
        {(['day', 'week', 'month', 'year'] as Timeframe[]).map((tf) => (
          <button
            key={tf}
            onClick={() => { setTimeframe(tf); setReferenceDate(new Date()); }}
            className={`px-3 py-1 text-xs rounded transition-colors ${
              timeframe === tf ? 'bg-[#818cf8] text-[#0d0d0f] font-medium' : 'text-[#a1a1aa] hover:bg-[#1f1f23]'
            }`}
          >
            {tf === 'day' ? 'Day' : tf === 'week' ? 'Week' : tf === 'month' ? 'Month' : 'Year'}
          </button>
        ))}
      </div>
    </div>
  );

  return (
    <Layout realtimeStatus={realtimeStatus} activeTab={activeTab} onTabChange={setActiveTab}>
      {activeTab === 'dashboard' && (
        <div className="flex flex-col gap-6">
          <div className="flex items-center gap-4">
            <div>
              <p className="text-xs text-[#6b7280]">System monitoring</p>
              <div className="flex items-center gap-3">
                <h1 className="text-2xl font-semibold text-[#f4f4f5]">Environmental Overview</h1>
                <div className="px-2 py-0.5 bg-[#16161a] border border-[#1f1f23] rounded text-[11px] text-[#6b7280] font-mono">
                  Sync {lastUpdateStr}
                </div>
              </div>
            </div>
          </div>
          <DashboardView
            readings={readings}
            latest={latest}
            logs={logs}
            timeframe={timeframe}
            timeRange={timeRange}
            loading={loading}
            presenceEvents={presenceEvents}
            formatSGTime={formatSGTime}
            comfortScore={comfortScore}
            displayData={displayData}
            periodNavigation={periodNavigation}
          />
        </div>
      )}

      {activeTab === 'power' && (
        <PowerView
          sessions={sessions}
          filteredSessions={filteredSessions}
          periodNavigation={periodNavigation}
        />
      )}
    </Layout>
  );
}
```

Changes: `Tab` type narrows to `'dashboard' | 'power'`; `SensorsView` import and its render branch are removed; `latest` is now passed to `DashboardView`; the header ("Environmental_OVR" → "Environmental Overview", "SYSTEM_MONITORING_UNIT" → "System monitoring") and `periodNavigation` (date/timeframe buttons) lose their uppercase/neon-cyan/glow styling in favor of the flat theme; timeframe toggle labels change from `DAY`/`WEEK`/`MONTH`/`YEAR` to `Day`/`Week`/`Month`/`Year`. All data-fetching, realtime subscription, comfort score, and presence-detection logic is untouched.

- [ ] **Step 4: Build to verify — this should now PASS**

Run: `cd dashboard; npm run build`
Expected: `SUCCESS` — this is the first fully-green build since Task 1, since every file referencing the old `KPICard` `color` prop, the old `Layout`/`Navigation` 3-tab API, and `SensorsView` has now been updated or removed.

- [ ] **Step 5: Commit**

```bash
git add dashboard/src/components/DashboardView.tsx dashboard/src/app/page.tsx
git rm dashboard/src/components/SensorsView.tsx
git commit -m "feat(dashboard): fold Sensors stats into Dashboard, remove Sensors tab"
```

---

### Task 8: PowerView restyle

**Files:**
- Modify: `dashboard/src/components/PowerView.tsx` (full replacement)

- [ ] **Step 1: Replace PowerView.tsx**

Replace the entire contents of `dashboard/src/components/PowerView.tsx` with:

```tsx
'use client';

import KPICard from '@/components/KPICard';
import { SessionVolatilityChart, UptimeAccumulationChart } from '@/components/SessionTable';
import SessionTable from '@/components/SessionTable';

interface Session {
  duration: number;
  synced_at: string;
  start_time: number;
  boot_count: number;
  measure_count: number;
  sleep_interval?: number;
}

interface PowerViewProps {
  sessions: Session[];
  filteredSessions: Session[];
  periodNavigation: React.ReactNode;
}

export default function PowerView({ sessions, filteredSessions, periodNavigation }: PowerViewProps) {
  const totalRuntime = sessions.reduce((acc, s) => acc + s.duration, 0);
  const hours = Math.floor(totalRuntime / 3600).toString().padStart(2, '0');
  const minutes = Math.floor((totalRuntime % 3600) / 60).toString().padStart(2, '0');
  const secs = (totalRuntime % 60).toString().padStart(2, '0');
  const meanSession = sessions.reduce((acc, s) => acc + s.duration, 0) / (sessions.length || 1);
  const efficiency = Math.min(100, Math.round((18 / meanSession) * 100));
  const maxBoot = Math.max(0, ...sessions.map((s) => s.boot_count));
  const maxMeasure = Math.max(0, ...sessions.map((s) => s.measure_count));
  const activeInterval = sessions[0]?.sleep_interval || 5;

  return (
    <div className="flex flex-col gap-8">
      <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-4">
        <div>
          <h1 className="text-2xl font-semibold text-[#f4f4f5]">Runtime Analytics</h1>
          <p className="text-xs text-[#6b7280]">Device longevity & sync efficiency</p>
        </div>
        <div className="inline-block px-3 py-1 bg-[#16161a] border border-[#1f1f23] rounded-full">
          <p className="text-[11px] text-[#a1a1aa]">Active profile: {activeInterval}m interval</p>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <KPICard
          icon="timer"
          label="Accumulated Uptime"
          value={`${hours}:${minutes}:${secs}`}
          unit=""
          progress={Math.min(100, (totalRuntime / 86400) * 100)}
          subLabel="Total hours across all sessions"
        />
        <KPICard
          icon="speed"
          label="Mean Session"
          value={meanSession.toFixed(1)}
          unit="s"
          progress={Math.min(100, (meanSession / 30) * 100)}
          subLabel="Average sync duration"
        />
        <KPICard
          icon="cycle"
          label="Device Lifecycle"
          value={`#${maxBoot}`}
          unit="Boots"
          progress={Math.min(100, (maxBoot / 100) * 100)}
          subLabel={`${maxMeasure} total syncs`}
        />
        <KPICard
          icon="verified"
          label="Efficiency Index"
          value={`${efficiency}`}
          unit="%"
          progress={efficiency}
          subLabel="vs 18s ideal benchmark"
        />
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="card p-6 min-h-[350px]">
          <h2 className="text-sm font-medium text-[#a1a1aa] mb-6">Session Volatility Index</h2>
          <SessionVolatilityChart sessions={filteredSessions} />
        </div>
        <div className="card p-6 min-h-[350px]">
          <h2 className="text-sm font-medium text-[#a1a1aa] mb-6">Uptime Accumulation Plot</h2>
          <UptimeAccumulationChart sessions={filteredSessions} />
        </div>
      </div>

      <div className="card p-6">
        <div className="flex flex-col xl:flex-row justify-between items-start xl:items-center gap-4 mb-6">
          <h2 className="text-lg font-medium text-[#f4f4f5]">Recent Syncs</h2>
          {periodNavigation}
        </div>
        <SessionTable sessions={filteredSessions} />
      </div>
    </div>
  );
}
```

Changes: `KPICard` calls drop the `color` prop; headings drop uppercase/neon styling ("Runtime_Analytics" → "Runtime Analytics", "Session_Volatility_Index" → "Session Volatility Index", etc.); the "Active Profile" pill drops its cyan border/glow for a flat neutral pill. All runtime/efficiency/boot-count calculations are untouched.

- [ ] **Step 2: Build to verify**

Run: `cd dashboard; npm run build`
Expected: `SUCCESS`.

- [ ] **Step 3: Commit**

```bash
git add dashboard/src/components/PowerView.tsx
git commit -m "feat(dashboard): flat restyle for PowerView"
```

---

### Task 9: Remove unused dependencies

**Files:**
- Modify: `dashboard/package.json`

- [ ] **Step 1: Remove recharts and lucide-react**

Both are confirmed to have zero imports anywhere in `dashboard/src/` (verified via `grep -r "recharts"` and `grep -r "lucide-react"` — only `echarts`/`echarts-for-react` are actually used for charts, and all icons use the `material-symbols-outlined` CSS class, not a React icon library).

In `dashboard/package.json`, find this exact block:

```json
    "@supabase/supabase-js": "^2.103.3",
    "@types/node-telegram-bot-api": "^0.64.14",
    "clsx": "^2.1.1",
    "echarts": "^6.0.0",
    "echarts-for-react": "^3.0.6",
    "framer-motion": "^12.38.0",
    "lucide-react": "^1.8.0",
    "next": "16.2.4",
    "node-telegram-bot-api": "^0.67.0",
    "react": "19.2.4",
    "react-dom": "19.2.4",
    "recharts": "^3.8.1",
    "tailwind-merge": "^3.5.0"
```

Replace it with:

```json
    "@supabase/supabase-js": "^2.103.3",
    "@types/node-telegram-bot-api": "^0.64.14",
    "clsx": "^2.1.1",
    "echarts": "^6.0.0",
    "echarts-for-react": "^3.0.6",
    "framer-motion": "^12.38.0",
    "next": "16.2.4",
    "node-telegram-bot-api": "^0.67.0",
    "react": "19.2.4",
    "react-dom": "19.2.4",
    "tailwind-merge": "^3.5.0"
```

- [ ] **Step 2: Reinstall to update the lockfile**

Run: `cd dashboard; npm install`
Expected: completes without error; `package-lock.json` updates to drop `recharts`/`lucide-react` and their sub-dependencies.

- [ ] **Step 3: Build to verify**

Run: `cd dashboard; npm run build`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add dashboard/package.json dashboard/package-lock.json
git commit -m "chore(dashboard): remove unused recharts and lucide-react dependencies"
```

---

### Task 10: Manual visual verification

**Files:** none (verification only)

- [ ] **Step 1: Run the lint check**

Run: `cd dashboard; npm run lint`
Expected: no errors (warnings about pre-existing patterns unrelated to this change are acceptable; do not introduce new lint errors).

- [ ] **Step 2: Start the dev server and visually inspect**

Run: `cd dashboard; npm run dev`
Open the printed local URL (typically `http://localhost:3000`) and confirm:
- The Dashboard tab shows 4 flat KPI cards (Temperature, Humidity, Light Intensity, Comfort Index) with no neon glow, a "Sensor Details" card with 9 stat chips including the Accelerometer readings, a Trend Analysis chart (indigo/zinc-gray lines, no shadow glow), Activity timeline, and System Logs — all in flat `#16161a` cards on a `#0d0d0f` background.
- The top bar shows only "Dashboard" and "Power" tabs inline — no sidebar, no mobile bottom nav bar at narrow viewport widths.
- The Power tab renders 4 KPI cards, two ECharts (Session Volatility, Uptime Accumulation) in indigo, and the Recent Syncs table.
- Clicking the date-range picker still opens a calendar dropdown, still restricted to dates ≥ the oldest recorded reading.
- Switching between Day/Week/Month/Year timeframe buttons still changes the data range.
- No `Sensors` tab exists anywhere in the UI.
- Battery voltage is not displayed anywhere.

- [ ] **Step 3: Confirm realtime updates still work**

If a physical device is actively syncing (or you can trigger a manual Supabase insert into `room_readings`), confirm the KPI values animate/update without a page refresh, and the connection status pill in the top-right shows "Online".

This step has no automated substitute — realtime behavior can only be observed by watching the live dashboard, which is why it's called out as its own manual step rather than folded into Step 2.
