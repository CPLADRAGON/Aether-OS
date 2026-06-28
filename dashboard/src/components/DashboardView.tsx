'use client';

import { motion } from 'framer-motion';
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

export default function DashboardView({
  readings,
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
  if (loading && readings.length === 0) {
    return (
      <div className="flex items-center justify-center min-h-[60vh]">
        <div className="flex flex-col items-center gap-4">
          <div className="w-12 h-12 border-4 border-[#00f3ff] border-t-transparent rounded-full animate-spin" />
          <p className="text-[#00f3ff] font-bold tracking-widest animate-pulse uppercase font-headline">
            Booting_Aether_OS
          </p>
        </div>
      </div>
    );
  }

  return (
    <>
      <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-4 gap-6">
        <KPICard
          icon="thermostat"
          label="TEMPERATURE"
          value={displayData.temp.toFixed(1)}
          unit="°C"
          color="cyan"
          progress={(displayData.temp / 50) * 100}
          subLabel={displayData.label}
        />
        <KPICard
          icon="humidity_percentage"
          label="HUMIDITY"
          value={displayData.hum.toFixed(1)}
          unit="%"
          color="magenta"
          progress={displayData.hum}
          subLabel={displayData.label}
        />
        <KPICard
          icon="light_mode"
          label="LIGHT INTENSITY"
          value={displayData.ldr.toFixed(0)}
          unit="LUX"
          color="lime"
          progress={(displayData.ldr / 4095) * 100}
          subLabel={displayData.label}
        />

        {/* Comfort Index Card */}
        <div className="glass-panel-heavy p-5 rounded-xl flex flex-col items-center justify-center gap-2 relative overflow-hidden group">
          <div className="absolute inset-0 bg-gradient-to-t from-[#00f3ff]/10 to-transparent opacity-50" />
          <p className="text-[10px] font-headline text-white/40 uppercase tracking-widest z-10">Comfort Index</p>
          <motion.div
            key={comfortScore}
            initial={{ scale: 0.8, opacity: 0.5 }}
            animate={{ scale: 1, opacity: 1 }}
            transition={{ type: 'spring', stiffness: 200, damping: 10 }}
            className="flex items-baseline gap-1 z-10"
          >
            <span className="text-5xl font-headline font-black text-white drop-shadow-[0_0_15px_rgba(255,255,255,0.3)]">
              {comfortScore}
            </span>
            <span className="text-sm text-white/40 font-body">/100</span>
          </motion.div>
          <div className="w-full bg-white/5 h-1.5 rounded-full mt-4 overflow-hidden z-10">
            <motion.div
              className="h-full progress-rainbow"
              initial={{ width: 0 }}
              animate={{ width: `${comfortScore}%` }}
              transition={{ duration: 1 }}
            />
          </div>
        </div>
      </div>

      {/* Trend Analysis + Side Panel */}
      <div className="grid grid-cols-1 xl:grid-cols-10 gap-6">
        {/* Chart (70%) */}
        <div className="xl:col-span-7">
          <div className="glass-panel-heavy p-6 rounded-xl min-h-[500px] flex flex-col">
            <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-4 mb-6">
              <div className="flex items-center gap-3">
                <div className="w-1 h-6 bg-[#00f3ff]" />
                <h2 className="text-lg font-headline font-bold uppercase tracking-wider">Trend_Analysis</h2>
              </div>
              <div className="flex items-center gap-2">{periodNavigation}</div>
            </div>
            <div className="flex-1 min-h-[350px]">
              <TrendChart readings={readings} timeRange={timeRange} timeframe={timeframe} />
            </div>
            <div className="flex gap-6 mt-6 border-t border-white/5 pt-4 overflow-x-auto">
              <LegendItem color="bg-[#00f3ff]" label="Temperature (°C)" />
              <LegendItem color="bg-[#cf5cff]" label="Humidity (%)" />
              <LegendItem color="bg-[#a4f200]" label="Light (Lux)" />
            </div>
          </div>
        </div>

        {/* Side Panel (30%) */}
        <div className="xl:col-span-3 flex flex-col gap-6">
          {/* Activity Timeline */}
          <div className="glass-panel-heavy p-6 rounded-xl min-h-[200px]">
            <div className="flex items-center gap-3 mb-6">
              <div className="w-1 h-6 bg-[#a4f200]" />
              <h3 className="font-headline text-lg font-bold uppercase tracking-wider">Activity</h3>
            </div>
            <ActivityTimeline events={presenceEvents} formatSGTime={formatSGTime} />
          </div>

          {/* System Logs */}
          <div className="glass-panel-heavy p-6 rounded-xl flex flex-col min-h-[250px]">
            <div className="flex items-center gap-3 mb-4">
              <div className="w-1 h-6 bg-[#cf5cff]" />
              <h3 className="font-headline text-lg font-bold uppercase tracking-wider">System Logs</h3>
            </div>
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
      <span className="text-[10px] text-white/40 uppercase font-bold tracking-wider">{label}</span>
    </div>
  );
}
