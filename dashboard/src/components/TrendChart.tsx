'use client';

/* eslint-disable @typescript-eslint/no-explicit-any */

import { useEffect, useMemo, useState } from 'react';
import ReactECharts from 'echarts-for-react';

interface Reading {
  id: number;
  created_at: string;
  temperature: number;
  humidity: number;
  ldr_value: number;
  lux_value: number;
  accel_total: number;
  battery_v: number;
  trigger_source: string;
}

interface TrendChartProps {
  readings: Reading[];
  timeRange: { start: Date; end: Date };
  timeframe: 'day' | 'week' | 'month' | 'year';
  prevReadings?: Reading[]; // optional previous period for compare mode
}

// Below this width, the chart drops the Humidity/Light axis LABELS (values
// are still fully plotted and available via tooltip) to reclaim the fixed
// ~80-130px right margin those labels need -- on a narrow phone card that
// margin eats a much bigger proportion of the available width than on
// desktop, squeezing the actual plot area into a visibly narrow strip.
const MOBILE_BREAKPOINT_PX = 640;

function useIsMobile(): boolean {
  const [isMobile, setIsMobile] = useState(false);
  useEffect(() => {
    const check = () => setIsMobile(window.innerWidth < MOBILE_BREAKPOINT_PX);
    check();
    window.addEventListener('resize', check);
    return () => window.removeEventListener('resize', check);
  }, []);
  return isMobile;
}

export default function TrendChart({ readings, timeRange, timeframe, prevReadings = [] }: TrendChartProps) {
  const isMobile = useIsMobile();
  const chartOptions = useMemo(() => {
    // Time-based moving average — smooths raw sensor jitter into a readable
    // trend line without shifting the curve forward/backward in time. Window
    // scales with the SELECTED TIMEFRAME (not raw sample count): a fixed
    // sample-count window means wildly different real time spans depending
    // on the device's sleep interval (5-60 min) and produces inconsistent
    // smoothing -- worse, capping the window at a small sample count meant
    // dense data (short sleep interval, week/month/year views) got barely
    // smoothed at all, since e.g. 15 samples at a 5-min interval is only
    // +-37.5 min of real smoothing for a whole WEEK-scale view. Using a
    // half-window defined in real minutes keeps smoothing strength
    // consistent and proportionate to what's actually on screen, regardless
    // of how densely the device happens to be sampling.
    const halfWindowMin =
      timeframe === 'day' ? 30 :
      timeframe === 'week' ? 180 :
      timeframe === 'month' ? 720 :
      4320; // year
    const times = readings.map((r) => new Date(r.created_at).getTime());
    const smooth = (values: number[]): number[] => {
      const n = values.length;
      if (n <= 2) return values; // too few points for any window to matter
      const halfWindowMs = halfWindowMin * 60 * 1000;
      const result: number[] = new Array(n);
      // Two-pointer sliding window (O(n) total) -- times[] is sorted
      // ascending (readings are fetched with `.order('created_at', {
      // ascending: true })`), so lo/hi only ever advance forward.
      let lo = 0, hi = 0, sum = 0;
      for (let i = 0; i < n; i++) {
        while (hi < n && times[hi] <= times[i] + halfWindowMs) { sum += values[hi]; hi++; }
        while (lo < i && times[lo] < times[i] - halfWindowMs) { sum -= values[lo]; lo++; }
        result[i] = sum / (hi - lo);
      }
      return result;
    };

    const temps = smooth(readings.map((r) => r.temperature));
    const hums = smooth(readings.map((r) => r.humidity));
    const luxs = smooth(readings.map((r) => r.lux_value || 0)); // guard pre-migration rows (null)

    // Compare mode: time-shift previous period data to align with the current
    // period so both periods overlay visually (same x-axis positions).
    const periodMs = timeRange.end.getTime() - timeRange.start.getTime();
    const prevTemps = prevReadings.length > 0 ? smooth(prevReadings.map((r) => r.temperature)) : [];
    const prevHums  = prevReadings.length > 0 ? smooth(prevReadings.map((r) => r.humidity)) : [];
    const prevLuxs  = prevReadings.length > 0 ? smooth(prevReadings.map((r) => r.lux_value || 0)) : [];

    // Dark-period shading intentionally uses raw ldr_value (NOT lux_value):
    // this is an internal display heuristic already tuned against raw ADC
    // behavior (threshold 100), not a human-facing value -- re-deriving it
    // against the nonlinear raw->lux conversion would require re-tuning
    // this threshold for no real benefit.
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
      grid: { left: '3%', right: isMobile ? 8 : 80, bottom: '3%', top: '10%', containLabel: true },
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
          // Tight dataMin/dataMax scaling exaggerates the DHT11's coarse
          // +-1C integer resolution: a room that's genuinely stable within
          // 1-2C would still stretch that whole range across the full
          // chart height, making ordinary sensor quantization steps look
          // like wild swings. A padded floor (at least 2C total headroom)
          // keeps small real ranges visually proportionate.
          min: (value: { min: number; max: number }) => {
            const pad = Math.max(1, (value.max - value.min) * 0.2);
            return Math.floor((value.min - pad) * 10) / 10;
          },
          max: (value: { min: number; max: number }) => {
            const pad = Math.max(1, (value.max - value.min) * 0.2);
            return Math.ceil((value.max + pad) * 10) / 10;
          },
        },
        {
          type: 'value',
          position: 'right',
          splitLine: { show: false },
          // Labels hidden on mobile to reclaim the fixed right-margin width
          // (see MOBILE_BREAKPOINT_PX comment) -- values are still fully
          // plotted and available on tap via the tooltip.
          axisLabel: { show: !isMobile, color: '#38bdf8', fontSize: 10, fontFamily: 'Inter, sans-serif', formatter: (val: number) => `${val.toFixed(1)}%` },
          // Same reasoning as the temperature axis above -- DHT11 humidity
          // resolution is also +-1% integer steps.
          min: (value: { min: number; max: number }) => {
            const pad = Math.max(3, (value.max - value.min) * 0.2);
            return Math.max(0, Math.floor(value.min - pad));
          },
          max: (value: { min: number; max: number }) => {
            const pad = Math.max(3, (value.max - value.min) * 0.2);
            return Math.min(100, Math.ceil(value.max + pad));
          },
        },
        {
          type: 'value',
          position: 'right',
          offset: isMobile ? 0 : 50,
          splitLine: { show: false },
          axisLabel: { show: !isMobile, color: '#facc15', fontSize: 10, fontFamily: 'Inter, sans-serif', formatter: (val: number) => `${val.toFixed(0)}lx` },
          min: 0,
          // Light naturally varies far more than temp/humidity (real
          // physical brightness changes, not sensor quantization), so it
          // doesn't need the same padding treatment -- just a little
          // headroom so peaks aren't flush against the top edge.
          max: (value: { max: number }) => Math.max(10, Math.ceil(value.max * 1.1)),
        },
      ],
      series: [
        {
          name: 'Temperature',
          type: 'line',
          smooth: 0.2,
          data: readings.map((r, i) => [new Date(r.created_at).getTime(), temps[i]]),
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
          smooth: 0.2,
          yAxisIndex: 1,
          data: readings.map((r, i) => [new Date(r.created_at).getTime(), hums[i]]),
          itemStyle: { color: '#38bdf8' },
          lineStyle: { width: 2 },
          showSymbol: false,
        },
        {
          name: 'Light',
          type: 'line',
          smooth: 0.2,
          yAxisIndex: 2,
          data: readings.map((r, i) => [new Date(r.created_at).getTime(), luxs[i]]),
          itemStyle: { color: '#facc15' },
          lineStyle: { width: 1.5, opacity: 0.85 },
          showSymbol: false,
        },
        // Compare-mode series: previous period, time-shifted to align with
        // the current period on the x-axis, rendered as dashed semi-transparent
        // lines on the same y-axes.
        ...(prevReadings.length > 0 ? [
          {
            name: 'Temp (prev)',
            type: 'line',
            smooth: 0.2,
            data: prevReadings.map((r, i) => [new Date(r.created_at).getTime() + periodMs, prevTemps[i]]),
            itemStyle: { color: '#818cf8' },
            lineStyle: { width: 1.5, type: 'dashed' as const, opacity: 0.35 },
            showSymbol: false,
            tooltip: { show: false } as any,
          },
          {
            name: 'Hum (prev)',
            type: 'line',
            smooth: 0.2,
            yAxisIndex: 1,
            data: prevReadings.map((r, i) => [new Date(r.created_at).getTime() + periodMs, prevHums[i]]),
            itemStyle: { color: '#38bdf8' },
            lineStyle: { width: 1, type: 'dashed' as const, opacity: 0.35 },
            showSymbol: false,
            tooltip: { show: false } as any,
          },
          {
            name: 'Light (prev)',
            type: 'line',
            smooth: 0.2,
            yAxisIndex: 2,
            data: prevReadings.map((r, i) => [new Date(r.created_at).getTime() + periodMs, prevLuxs[i]]),
            itemStyle: { color: '#facc15' },
            lineStyle: { width: 1, type: 'dashed' as const, opacity: 0.35 },
            showSymbol: false,
            tooltip: { show: false } as any,
          },
        ] : []),
      ],
    };
  }, [readings, timeRange, timeframe, isMobile, prevReadings]);

  return (
    <ReactECharts option={chartOptions} style={{ height: '100%', width: '100%' }} opts={{ renderer: 'svg' }} />
  );
}
