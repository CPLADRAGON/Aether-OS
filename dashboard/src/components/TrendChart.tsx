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
    // Centered moving average — smooths raw sensor jitter into a readable
    // trend line without shifting the curve forward/backward in time (unlike
    // a trailing average). Window scales with data density: sparse data (few
    // readings) stays essentially untouched, dense data (week/month/year
    // views) gets smoothed more so the underlying trend reads clearly instead
    // of a jittery mess of tiny fluctuations.
    const windowSize = Math.max(1, Math.min(15, Math.round(readings.length / 40)));
    const smooth = (values: number[]): number[] => {
      if (windowSize <= 1) return values;
      const result: number[] = [];
      for (let i = 0; i < values.length; i++) {
        const start = Math.max(0, i - Math.floor(windowSize / 2));
        const end = Math.min(values.length, i + Math.ceil(windowSize / 2));
        let sum = 0;
        for (let j = start; j < end; j++) sum += values[j];
        result.push(sum / (end - start));
      }
      return result;
    };

    const temps = smooth(readings.map((r) => r.temperature));
    const hums = smooth(readings.map((r) => r.humidity));
    const ldrs = smooth(readings.map((r) => r.ldr_value));

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
          axisLabel: { color: '#38bdf8', fontSize: 10, fontFamily: 'Inter, sans-serif', formatter: (val: number) => `${val.toFixed(1)}%` },
          min: 'dataMin',
          max: 'dataMax',
        },
        {
          type: 'value',
          position: 'right',
          offset: 50,
          splitLine: { show: false },
          axisLabel: { color: '#facc15', fontSize: 10, fontFamily: 'Inter, sans-serif', formatter: (val: number) => `${val.toFixed(0)}lx` },
          min: 0,
          max: 'dataMax',
        },
      ],
      series: [
        {
          name: 'Temperature',
          type: 'line',
          smooth: true,
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
          smooth: true,
          yAxisIndex: 1,
          data: readings.map((r, i) => [new Date(r.created_at).getTime(), hums[i]]),
          itemStyle: { color: '#38bdf8' },
          lineStyle: { width: 2 },
          showSymbol: false,
        },
        {
          name: 'Light',
          type: 'line',
          smooth: true,
          yAxisIndex: 2,
          data: readings.map((r, i) => [new Date(r.created_at).getTime(), ldrs[i]]),
          itemStyle: { color: '#facc15' },
          lineStyle: { width: 1.5, opacity: 0.85 },
          showSymbol: false,
        },
      ],
    };
  }, [readings, timeRange, timeframe]);

  return (
    <ReactECharts option={chartOptions} style={{ height: '100%', width: '100%' }} opts={{ renderer: 'svg' }} />
  );
}
