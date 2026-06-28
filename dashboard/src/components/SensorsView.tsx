'use client';

import { useMemo } from 'react';

interface Reading {
  id: number;
  created_at: string;
  temperature: number;
  humidity: number;
  ldr_value: number;
  accel_total: number;
  battery_v: number;
}

interface SensorsViewProps {
  readings: Reading[];
  latest: Reading | null;
  formatSGTime: (dateStr: string) => string;
}

interface SensorCardProps {
  icon: string;
  label: string;
  value: string;
  unit: string;
  status: string;
  statusColor: 'green' | 'cyan' | 'amber';
  min: string;
  max: string;
  avg: string;
  accentColor: string;
}

function SensorCard({ icon, label, value, unit, status, statusColor, min, max, avg, accentColor }: SensorCardProps) {
  const statusColors = {
    green: 'text-emerald-500 bg-emerald-500/10 border-emerald-500/20',
    cyan: 'text-[#00f3ff] bg-[#00f3ff]/10 border-[#00f3ff]/20',
    amber: 'text-amber-400 bg-amber-400/10 border-amber-400/20',
  };

  return (
    <div className={`glass-panel-heavy rounded-xl p-6 flex flex-col gap-4 border-l-4`} style={{ borderLeftColor: accentColor }}>
      <div className="flex justify-between items-start">
        <div>
          <p className="text-[10px] font-headline text-white/40 uppercase tracking-widest">{label}</p>
          <div className="flex items-baseline gap-1 mt-1">
            <span className="text-3xl font-headline font-bold" style={{ color: accentColor }}>
              {value}
            </span>
            <span className="text-sm text-white/40 font-body">{unit}</span>
          </div>
        </div>
        <span className="material-symbols-outlined text-2xl" style={{ color: accentColor }}>
          {icon}
        </span>
      </div>

      {/* Status Badge */}
      <div className={`inline-flex items-center gap-1.5 px-2 py-0.5 rounded-full text-[10px] font-bold uppercase tracking-wider self-start ${statusColors[statusColor]}`}>
        <span className={`w-1.5 h-1.5 rounded-full ${statusColor === 'green' ? 'bg-emerald-500' : statusColor === 'cyan' ? 'bg-[#00f3ff]' : 'bg-amber-400'}`} />
        {status}
      </div>

      {/* Stats Row */}
      <div className="grid grid-cols-3 gap-4 bg-black/20 rounded-lg p-3 border border-white/5">
        <div className="text-center">
          <p className="text-[9px] text-white/30 uppercase tracking-wider">Min</p>
          <p className="text-xs font-mono text-white/60">{min}</p>
        </div>
        <div className="text-center border-x border-white/5">
          <p className="text-[9px] text-white/30 uppercase tracking-wider">Max</p>
          <p className="text-xs font-mono text-white/60">{max}</p>
        </div>
        <div className="text-center">
          <p className="text-[9px] text-white/30 uppercase tracking-wider">Avg</p>
          <p className="text-xs font-mono text-white/60">{avg}</p>
        </div>
      </div>
    </div>
  );
}

export default function SensorsView({ readings, latest, formatSGTime }: SensorsViewProps) {
  const stats = useMemo(() => {
    if (readings.length === 0) return null;
    const temps = readings.map((r) => r.temperature);
    const hums = readings.map((r) => r.humidity);
    const ldrs = readings.map((r) => r.ldr_value);
    const accels = readings.map((r) => r.accel_total);
    const bats = readings.map((r) => r.battery_v);

    const avg = (arr: number[]) => arr.reduce((a, b) => a + b, 0) / arr.length;

    return {
      temp: { min: Math.min(...temps), max: Math.max(...temps), avg: avg(temps) },
      hum: { min: Math.min(...hums), max: Math.max(...hums), avg: avg(hums) },
      ldr: { min: Math.min(...ldrs), max: Math.max(...ldrs), avg: avg(ldrs) },
      accel: { min: Math.min(...accels), max: Math.max(...accels), avg: avg(accels) },
      bat: { min: Math.min(...bats), max: Math.max(...bats), avg: avg(bats) },
    };
  }, [readings]);

  if (!latest || !stats) {
    return (
      <div className="flex items-center justify-center min-h-[60vh]">
        <p className="text-white/40 text-sm italic font-mono">No sensor data available yet.</p>
      </div>
    );
  }

  const sensors: SensorCardProps[] = [
    {
      icon: 'thermostat',
      label: 'TEMPERATURE',
      value: latest.temperature.toFixed(1),
      unit: '°C',
      status: latest.temperature > 15 && latest.temperature < 35 ? 'NOMINAL' : 'CAUTION',
      statusColor: latest.temperature > 15 && latest.temperature < 35 ? 'green' : 'amber',
      min: stats.temp.min.toFixed(1) + '°C',
      max: stats.temp.max.toFixed(1) + '°C',
      avg: stats.temp.avg.toFixed(1) + '°C',
      accentColor: '#00f3ff',
    },
    {
      icon: 'humidity_percentage',
      label: 'HUMIDITY',
      value: latest.humidity.toFixed(1),
      unit: '%',
      status: latest.humidity > 30 && latest.humidity < 80 ? 'NOMINAL' : 'CAUTION',
      statusColor: latest.humidity > 30 && latest.humidity < 80 ? 'green' : 'amber',
      min: stats.hum.min.toFixed(1) + '%',
      max: stats.hum.max.toFixed(1) + '%',
      avg: stats.hum.avg.toFixed(1) + '%',
      accentColor: '#cf5cff',
    },
    {
      icon: 'light_mode',
      label: 'LIGHT INTENSITY',
      value: latest.ldr_value.toFixed(0),
      unit: 'LUX',
      status: 'NOMINAL',
      statusColor: 'green',
      min: stats.ldr.min.toFixed(0) + ' lx',
      max: stats.ldr.max.toFixed(0) + ' lx',
      avg: stats.ldr.avg.toFixed(0) + ' lx',
      accentColor: '#a4f200',
    },
    {
      icon: 'vibration',
      label: 'ACCELEROMETER',
      value: latest.accel_total.toFixed(2),
      unit: 'm/s²',
      status: latest.accel_total < 12 ? 'STABLE' : 'MOVEMENT',
      statusColor: 'cyan',
      min: stats.accel.min.toFixed(2) + ' m/s²',
      max: stats.accel.max.toFixed(2) + ' m/s²',
      avg: stats.accel.avg.toFixed(2) + ' m/s²',
      accentColor: '#ffffff',
    },
    {
      icon: 'battery_full',
      label: 'BATTERY',
      value: latest.battery_v.toFixed(2),
      unit: 'V',
      status: latest.battery_v > 3.0 ? 'GOOD' : latest.battery_v > 2.5 ? 'LOW' : 'CRITICAL',
      statusColor: latest.battery_v > 3.0 ? 'green' : latest.battery_v > 2.5 ? 'amber' : 'amber',
      min: stats.bat.min.toFixed(2) + 'V',
      max: stats.bat.max.toFixed(2) + 'V',
      avg: stats.bat.avg.toFixed(2) + 'V',
      accentColor: '#f59e0b',
    },
  ];

  return (
    <div className="flex flex-col gap-6">
      <div>
        <h1 className="text-2xl md:text-3xl font-headline font-semibold text-white uppercase tracking-tight">
          Sensor_Array
        </h1>
        <p className="text-[12px] text-[#00f3ff]/70 uppercase tracking-[0.3em] font-body">
          Real-Time Device Telemetry
        </p>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {sensors.map((sensor) => (
          <SensorCard key={sensor.label} {...sensor} />
        ))}
      </div>

      <div className="glass-panel-heavy p-4 rounded-xl">
        <p className="text-[10px] text-white/30 font-mono text-center">
          Last updated: {formatSGTime(latest.created_at)} — {readings.length} readings in current period
        </p>
      </div>
    </div>
  );
}
