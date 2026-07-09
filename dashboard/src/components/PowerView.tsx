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
