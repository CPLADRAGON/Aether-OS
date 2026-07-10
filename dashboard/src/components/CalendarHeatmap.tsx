'use client';

import { useMemo } from 'react';
import { motion } from 'framer-motion';

interface Reading {
  created_at: string;
  temperature: number;
  humidity: number;
}

interface CalendarHeatmapProps {
  readings: Reading[];
}

// Maps a comfort score (0–100) to a Tailwind background class on a 5-level
// indigo scale matching the dashboard's existing colour palette.
function scoreToColor(score: number | null): string {
  if (score === null) return 'bg-[#1f1f23]';
  if (score >= 80) return 'bg-[#818cf8]';
  if (score >= 60) return 'bg-[#6366f1]';
  if (score >= 40) return 'bg-[#4338ca]';
  if (score >= 20) return 'bg-[#312e81]';
  return 'bg-[#1e1b4b]';
}

function comfortScore(temp: number, hum: number): number {
  const tScore = Math.max(0, Math.min(100, 100 - Math.abs(temp - 24) * 5));
  const hScore = Math.max(0, Math.min(100, 100 - Math.abs(hum - 50) * 1.5));
  return Math.round((tScore + hScore) / 2);
}

export default function CalendarHeatmap({ readings }: CalendarHeatmapProps) {
  const dailyScores = useMemo(() => {
    const map: Record<string, { totalT: number; totalH: number; count: number }> = {};
    for (const r of readings) {
      const day = new Date(r.created_at).toLocaleDateString('en-CA', { timeZone: 'Asia/Singapore' }); // YYYY-MM-DD
      if (!map[day]) map[day] = { totalT: 0, totalH: 0, count: 0 };
      map[day].totalT += r.temperature;
      map[day].totalH += r.humidity;
      map[day].count++;
    }
    const result: Record<string, { score: number; avgTemp: number; avgHum: number }> = {};
    for (const [day, v] of Object.entries(map)) {
      const avgT = v.totalT / v.count;
      const avgH = v.totalH / v.count;
      result[day] = { score: comfortScore(avgT, avgH), avgTemp: avgT, avgHum: avgH };
    }
    return result;
  }, [readings]);

  // Build a 12-week rolling window ending today (SGT)
  const weeks = useMemo(() => {
    const today = new Date(new Date().toLocaleString('en-US', { timeZone: 'Asia/Singapore' }));
    today.setHours(0, 0, 0, 0);
    // Align to Monday of current week
    const dow = (today.getDay() + 6) % 7; // 0=Mon
    const weekStart = new Date(today);
    weekStart.setDate(today.getDate() - dow - 11 * 7); // 12 weeks back

    const grid: { date: string; label: string }[][] = [];
    for (let w = 0; w < 12; w++) {
      const week: { date: string; label: string }[] = [];
      for (let d = 0; d < 7; d++) {
        const day = new Date(weekStart);
        day.setDate(weekStart.getDate() + w * 7 + d);
        const iso = day.toLocaleDateString('en-CA', { timeZone: 'Asia/Singapore' });
        const label = day.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', day: 'numeric' });
        week.push({ date: iso, label });
      }
      grid.push(week);
    }
    return grid;
  }, []);

  // Month labels for the column headers
  const monthLabels = useMemo(() => {
    return weeks.map((week) => {
      const firstDay = new Date(week[0].date + 'T00:00:00');
      if (firstDay.getDate() <= 7) {
        return firstDay.toLocaleDateString('en-SG', { month: 'short' });
      }
      return '';
    });
  }, [weeks]);

  const dayLabels = ['M', 'T', 'W', 'T', 'F', 'S', 'S'];

  if (readings.length === 0) return null;

  return (
    <div className="card p-4 sm:p-5">
      <div className="flex items-center justify-between mb-3">
        <p className="text-xs text-[#6b7280]">Comfort heatmap (12 weeks)</p>
        <div className="flex items-center gap-1.5">
          <span className="text-[10px] text-[#6b7280]">Low</span>
          {['bg-[#1e1b4b]', 'bg-[#312e81]', 'bg-[#4338ca]', 'bg-[#6366f1]', 'bg-[#818cf8]'].map((c, i) => (
            <div key={i} className={`w-2.5 h-2.5 rounded-sm ${c}`} />
          ))}
          <span className="text-[10px] text-[#6b7280]">High</span>
        </div>
      </div>

      <div className="flex gap-1">
        {/* Day labels column */}
        <div className="flex flex-col gap-0.5 pt-4">
          {dayLabels.map((d, i) => (
            <div key={i} className="w-4 h-3 flex items-center justify-end pr-1">
              {(i % 2 === 0) && <span className="text-[9px] text-[#4b5563]">{d}</span>}
            </div>
          ))}
        </div>

        {/* Week columns */}
        <div className="flex gap-0.5 overflow-x-auto">
          {weeks.map((week, wi) => (
            <div key={wi} className="flex flex-col gap-0.5">
              {/* Month label */}
              <div className="h-4 flex items-center">
                <span className="text-[9px] text-[#4b5563] whitespace-nowrap">{monthLabels[wi]}</span>
              </div>
              {/* Day cells */}
              {week.map((day) => {
                const entry = dailyScores[day.date];
                const score = entry?.score ?? null;
                return (
                  <motion.div
                    key={day.date}
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    transition={{ delay: 0.01 }}
                    title={entry
                      ? `${day.label}: comfort ${score}, ${entry.avgTemp.toFixed(1)}°C, ${entry.avgHum.toFixed(0)}%`
                      : `${day.label}: no data`}
                    className={`w-3 h-3 rounded-sm ${scoreToColor(score)} cursor-default`}
                  />
                );
              })}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
