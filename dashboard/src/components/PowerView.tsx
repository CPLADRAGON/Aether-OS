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
      {/* Header */}
      <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-4">
        <div>
          <h1 className="text-2xl md:text-3xl font-headline font-semibold text-white uppercase tracking-tight">
            Runtime_Analytics
          </h1>
          <p className="text-[12px] text-[#00f3ff]/70 uppercase tracking-[0.3em] font-body">
            Device Longevity & Sync Efficiency
          </p>
        </div>
        <div className="inline-block px-3 py-1 bg-[#00f3ff]/10 border border-[#00f3ff]/20 rounded-full">
          <p className="text-[10px] text-[#00f3ff] font-bold uppercase tracking-wider">
            Active Profile: {activeInterval}m Interval
          </p>
        </div>
      </div>

      {/* KPI Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <KPICard
          icon="timer"
          label="ACCUMULATED UPTIME"
          value={`${hours}:${minutes}:${secs}`}
          unit=""
          color="cyan"
          progress={Math.min(100, (totalRuntime / 86400) * 100)}
          subLabel="Total hours across all sessions"
        />
        <KPICard
          icon="speed"
          label="MEAN SESSION"
          value={meanSession.toFixed(1)}
          unit="s"
          color="magenta"
          progress={Math.min(100, (meanSession / 30) * 100)}
          subLabel="Average sync duration"
        />
        <KPICard
          icon="cycle"
          label="DEVICE LIFECYCLE"
          value={`#${maxBoot}`}
          unit={`Boots`}
          color="amber"
          progress={Math.min(100, (maxBoot / 100) * 100)}
          subLabel={`${maxMeasure} total syncs`}
        />
        <KPICard
          icon="verified"
          label="EFFICIENCY INDEX"
          value={`${efficiency}`}
          unit="%"
          color="lime"
          progress={efficiency}
          subLabel="vs 18s ideal benchmark"
        />
      </div>

      {/* Charts */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="glass-panel-heavy p-6 rounded-xl min-h-[350px]">
          <h2 className="text-[12px] font-headline font-bold text-white/60 uppercase tracking-widest mb-6">
            Session_Volatility_Index
          </h2>
          <SessionVolatilityChart sessions={filteredSessions} />
        </div>
        <div className="glass-panel-heavy p-6 rounded-xl min-h-[350px]">
          <h2 className="text-[12px] font-headline font-bold text-white/60 uppercase tracking-widest mb-6">
            Uptime_Accumulation_Plot
          </h2>
          <UptimeAccumulationChart sessions={filteredSessions} />
        </div>
      </div>

      {/* Sessions Table */}
      <div className="glass-panel-heavy p-6 rounded-xl">
        <div className="flex flex-col xl:flex-row justify-between items-start xl:items-center gap-4 mb-6">
          <h2 className="text-xl font-headline font-medium text-white uppercase tracking-tight">Recent_Syncs</h2>
          {periodNavigation}
        </div>
        <SessionTable sessions={filteredSessions} />
      </div>
    </div>
  );
}
