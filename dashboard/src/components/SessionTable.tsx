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
