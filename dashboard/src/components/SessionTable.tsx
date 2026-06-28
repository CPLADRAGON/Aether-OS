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
          axisLabel: { color: '#849495', fontSize: 10 },
        },
        yAxis: {
          type: 'value',
          splitLine: { lineStyle: { color: 'rgba(255,255,255,0.05)' } },
          axisLabel: { color: '#849495', fontSize: 10 },
        },
        series: [
          {
            data: data.map((s) => ({
              value: s.duration,
              itemStyle: { color: s.duration > 25 ? '#FBBF24' : '#00f3ff' },
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
          axisLabel: { color: '#849495', fontSize: 10 },
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
            lineStyle: { color: '#00f3ff', width: 3, shadowBlur: 10, shadowColor: '#00f3ff' },
            areaStyle: {
              color: {
                type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                colorStops: [
                  { offset: 0, color: 'rgba(0,243,255,0.3)' },
                  { offset: 1, color: 'rgba(0,243,255,0)' },
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
        <p className="text-white/30 text-xs italic font-mono">No session data for this period.</p>
      </div>
    );
  }

  return (
    <div className="overflow-x-auto">
      <table className="w-full text-left border-collapse">
        <thead>
          <tr className="border-b border-white/10 text-[10px] text-white/40 tracking-widest uppercase">
            <th className="py-4 px-2 font-medium">Timestamp (SGT)</th>
            <th className="py-4 px-2 font-medium">Duration</th>
            <th className="py-4 px-2 font-medium">Boot Index</th>
            <th className="py-4 px-2 font-medium">Sync Count</th>
            <th className="py-4 px-2 font-medium">Efficiency</th>
          </tr>
        </thead>
        <tbody className="text-xs font-mono">
          {sessions.map((s, idx) => (
            <tr key={idx} className="border-b border-white/5 hover:bg-white/5 transition-colors">
              <td className="py-4 px-2 text-white/60">
                {new Date(s.synced_at).toLocaleString('en-SG', { timeZone: 'Asia/Singapore' })}
              </td>
              <td className="py-4 px-2 text-[#00f3ff]">{s.duration}s</td>
              <td className="py-4 px-2 text-amber-400">#{s.boot_count}</td>
              <td className="py-4 px-2 text-[#cf5cff]">{s.measure_count}</td>
              <td className="py-4 px-2">
                <div className="w-24 bg-white/5 h-1.5 rounded-full overflow-hidden">
                  <div
                    className="h-full bg-emerald-500 rounded-full"
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
