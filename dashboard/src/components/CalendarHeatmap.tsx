'use client';

import { useMemo } from 'react';

interface Reading {
  created_at: string;
  temperature: number;
  humidity: number;
}

interface CalendarHeatmapProps {
  readings: Reading[];
}

function comfortScore(temp: number, hum: number): number {
  const tScore = Math.max(0, Math.min(100, 100 - Math.abs(temp - 24) * 5));
  const hScore = Math.max(0, Math.min(100, 100 - Math.abs(hum - 50) * 1.5));
  return Math.round((tScore + hScore) / 2);
}

function scoreToColor(score: number | null): string {
  if (score === null) return 'bg-[#1f1f23]';
  if (score >= 80) return 'bg-[#818cf8]';
  if (score >= 60) return 'bg-[#6366f1]';
  if (score >= 40) return 'bg-[#4338ca]';
  if (score >= 20) return 'bg-[#312e81]';
  return 'bg-[#1e1b4b]';
}

export default function CalendarHeatmap({ readings }: CalendarHeatmapProps) {
  const dailyScores = useMemo(() => {
    const map: Record<string, { totalT: number; totalH: number; count: number }> = {};
    for (const r of readings) {
      const day = new Date(r.created_at).toLocaleDateString('en-CA', { timeZone: 'Asia/Singapore' });
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

  // Full-year grid: week columns from Jan 1 to today (SGT).
  // Future days render transparent to preserve the full-year shape.
  const { weeks, monthLabels, todayStr } = useMemo(() => {
    const todaySGT = new Date(new Date().toLocaleString('en-US', { timeZone: 'Asia/Singapore' }));
    todaySGT.setHours(0, 0, 0, 0);
    const todayISO = todaySGT.toLocaleDateString('en-CA');

    // Align grid start to Monday of the week that contains Jan 1
    const jan1 = new Date(todaySGT.getFullYear(), 0, 1);
    const jan1Dow = (jan1.getDay() + 6) % 7; // 0=Mon
    const gridStart = new Date(jan1);
    gridStart.setDate(jan1.getDate() - jan1Dow);

    const weeksGrid: { date: string; isFuture: boolean; label: string }[][] = [];
    const mlabels: string[] = [];

    for (let col = 0; col < 54; col++) {
      const weekStart = new Date(gridStart);
      weekStart.setDate(gridStart.getDate() + col * 7);
      // Stop once the whole week is past Dec 31
      if (weekStart.getFullYear() > todaySGT.getFullYear()) break;

      const week: { date: string; isFuture: boolean; label: string }[] = [];
      for (let d = 0; d < 7; d++) {
        const day = new Date(weekStart);
        day.setDate(weekStart.getDate() + d);
        const iso = day.toLocaleDateString('en-CA');
        week.push({
          date: iso,
          isFuture: iso > todayISO,
          label: day.toLocaleDateString('en-SG', { month: 'short', day: 'numeric' }),
        });
      }
      weeksGrid.push(week);

      // Show month name on the first week that starts in a new month
      const mon0 = week[0];
      const d0 = new Date(mon0.date + 'T00:00');
      mlabels.push(d0.getDate() <= 7 ? d0.toLocaleDateString('en-SG', { month: 'short' }) : '');
    }

    return { weeks: weeksGrid, monthLabels: mlabels, todayStr: todayISO };
  }, []);

  if (readings.length === 0) return null;

  const dayLabels = ['M', '', 'W', '', 'F', '', 'S'];

  return (
    <div className="card p-4 sm:p-5">
      <div className="flex items-center justify-between mb-3 gap-2 flex-wrap">
        <p className="text-xs text-[#6b7280]">Comfort heatmap ({new Date().getFullYear()})</p>
        <div className="flex items-center gap-1.5 shrink-0">
          <span className="text-[10px] text-[#6b7280]">Low</span>
          {['bg-[#1e1b4b]', 'bg-[#312e81]', 'bg-[#4338ca]', 'bg-[#6366f1]', 'bg-[#818cf8]'].map((c, i) => (
            <div key={i} className={`w-2.5 h-2.5 rounded-sm ${c}`} />
          ))}
          <span className="text-[10px] text-[#6b7280]">High</span>
        </div>
      </div>

      {/* Scrollable — 52+ columns × ~14px each needs horizontal scroll on phone */}
      <div className="overflow-x-auto -mx-1 px-1 pb-1">
        <div className="flex gap-0.5" style={{ minWidth: `${weeks.length * 14}px` }}>
          {/* Day-of-week labels */}
          <div className="flex flex-col gap-0.5 pt-4 mr-0.5 shrink-0">
            {dayLabels.map((d, i) => (
              <div key={i} className="w-3 h-3 flex items-center justify-end">
                <span className="text-[8px] leading-none text-[#4b5563]">{d}</span>
              </div>
            ))}
          </div>

          {/* Week columns */}
          {weeks.map((week, wi) => (
            <div key={wi} className="flex flex-col gap-0.5 shrink-0">
              {/* Month label row */}
              <div className="h-4 flex items-end pb-0.5">
                <span className="text-[8px] leading-none text-[#4b5563] whitespace-nowrap">{monthLabels[wi]}</span>
              </div>
              {/* Day cells */}
              {week.map((day) => {
                const entry = dailyScores[day.date];
                const score = (!day.isFuture && entry) ? entry.score : null;
                const isToday = day.date === todayStr;
                return (
                  <div
                    key={day.date}
                    title={
                      day.isFuture ? '' :
                      entry ? `${day.label}: comfort ${score}, ${entry.avgTemp.toFixed(1)}°C, ${entry.avgHum.toFixed(0)}%` :
                      `${day.label}: no data`
                    }
                    className={[
                      'w-3 h-3 rounded-sm cursor-default',
                      day.isFuture ? 'bg-transparent' : scoreToColor(score),
                      isToday ? 'ring-1 ring-[#818cf8] ring-offset-[1px] ring-offset-[#0d0d0f]' : '',
                    ].join(' ')}
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
