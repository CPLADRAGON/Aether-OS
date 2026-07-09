'use client';

import { motion } from 'framer-motion';
import { useMemo } from 'react';
import KPICard from '@/components/KPICard';
import TrendChart from '@/components/TrendChart';
import ActivityTimeline from '@/components/ActivityTimeline';
import SystemLogs from '@/components/SystemLogs';
import { lightLevelDetail } from '@/lib/light';

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
  displayData: { temp: number; hum: number; lux: number; label: string };
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
    const luxs = readings.map((r) => r.lux_value || 0); // guard pre-migration rows (null)
    const accels = readings.map((r) => r.accel_total);
    return {
      temp: { min: Math.min(...temps), max: Math.max(...temps), avg: avg(temps) },
      hum: { min: Math.min(...hums), max: Math.max(...hums), avg: avg(hums) },
      lux: { min: Math.min(...luxs), max: Math.max(...luxs), avg: avg(luxs) },
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
      <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-4 gap-4 sm:gap-6">
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
          value={displayData.lux.toFixed(0)}
          unit="lux"
          progress={(displayData.lux / 1500) * 100}
          subLabel={`${displayData.label} · ${lightLevelDetail(displayData.lux)}`}
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
        <div className="card p-4 sm:p-5">
          <p className="text-xs text-[#6b7280] mb-3">Sensor Details</p>
          <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-5 gap-2">
            <StatChip label="Temp Min" value={`${stats.temp.min.toFixed(1)}°C`} />
            <StatChip label="Temp Max" value={`${stats.temp.max.toFixed(1)}°C`} />
            <StatChip label="Temp Avg" value={`${stats.temp.avg.toFixed(1)}°C`} />
            <StatChip label="Humidity Avg" value={`${stats.hum.avg.toFixed(1)}%`} />
            <StatChip label="Light Avg" value={`${stats.lux.avg.toFixed(0)} lx`} />
            <StatChip label="Accelerometer" value={`${latest.accel_total.toFixed(2)} m/s²`} />
            <StatChip label="Accel Min" value={`${stats.accel.min.toFixed(2)} m/s²`} />
            <StatChip label="Accel Max" value={`${stats.accel.max.toFixed(2)} m/s²`} />
            <StatChip label="Accel Avg" value={`${stats.accel.avg.toFixed(2)} m/s²`} />
          </div>
        </div>
      )}

      <div className="grid grid-cols-1 xl:grid-cols-10 gap-4 sm:gap-6">
        <div className="xl:col-span-7">
          <div className="card p-4 sm:p-6 flex flex-col">
            <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-3 mb-4 sm:mb-6">
              <h2 className="text-sm font-medium text-[#a1a1aa]">Trend Analysis</h2>
              <div className="flex flex-wrap items-center gap-2">{periodNavigation}</div>
            </div>
            <div className="h-[260px] sm:h-[320px] md:h-[380px]">
              <TrendChart readings={readings} timeRange={timeRange} timeframe={timeframe} />
            </div>
            <div className="flex flex-wrap gap-4 sm:gap-6 mt-4 sm:mt-6 border-t border-[#1f1f23] pt-4 overflow-x-auto">
              <LegendItem color="bg-[#818cf8]" label="Temperature (°C)" />
              <LegendItem color="bg-[#38bdf8]" label="Humidity (%)" />
              <LegendItem color="bg-[#facc15]" label="Light (Lux)" />
            </div>
          </div>
        </div>

        <div className="xl:col-span-3 flex flex-col gap-4 sm:gap-6">
          <div className="card p-4 sm:p-6">
            <h3 className="text-sm font-medium text-[#a1a1aa] mb-4 sm:mb-6">Activity</h3>
            <ActivityTimeline events={presenceEvents} formatSGTime={formatSGTime} />
          </div>

          <div className="card p-4 sm:p-6 flex flex-col">
            <h3 className="text-sm font-medium text-[#a1a1aa] mb-3 sm:mb-4">System Logs</h3>
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
