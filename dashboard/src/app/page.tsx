'use client';

import { useCallback, useEffect, useState, useMemo } from 'react';
import DatePicker from '@/components/DatePicker';
import Layout from '@/components/Layout';
import DashboardView from '@/components/DashboardView';
import PowerView from '@/components/PowerView';
import { supabase } from '@/lib/supabase';

// Polyfill for crypto.randomUUID (Required for non-secure IP access)
if (typeof window !== 'undefined' && !window.crypto.randomUUID) {
  // @ts-expect-error - polyfill for environments without crypto.randomUUID
  window.crypto.randomUUID = () => {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
      const r = (Math.random() * 16) | 0;
      const v = c === 'x' ? r : (r & 0x3) | 0x8;
      return v.toString(16);
    });
  };
}

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

interface Session {
  duration: number;
  synced_at: string;
  start_time: number;
  boot_count: number;
  measure_count: number;
  sleep_interval?: number;
}

type Timeframe = 'day' | 'week' | 'month' | 'year';
type Tab = 'dashboard' | 'power';

export default function Dashboard() {
  // State
  const [readings, setReadings] = useState<Reading[]>([]);
  const [latest, setLatest] = useState<Reading | null>(null);
  const [logs, setLogs] = useState<DeviceLog[]>([]);
  const [sessions, setSessions] = useState<Session[]>([]);
  const [loading, setLoading] = useState(true);
  const [timeframe, setTimeframe] = useState<Timeframe>('day');
  const [referenceDate, setReferenceDate] = useState<Date>(new Date());
  const [isDatePickerOpen, setIsDatePickerOpen] = useState(false);
  const [oldestDate, setOldestDate] = useState<Date | null>(null);
  const [realtimeStatus, setRealtimeStatus] = useState<'connecting' | 'online' | 'offline'>('connecting');
  const [activeTab, setActiveTab] = useState<Tab>('dashboard');

  // Time range calculation
  const timeRange = useMemo(() => {
    const start = new Date(referenceDate);
    const end = new Date(referenceDate);
    start.setHours(0, 0, 0, 0);
    end.setHours(23, 59, 59, 999);
    if (timeframe === 'week') {
      let currentDay = start.getDay();
      if (currentDay === 0) currentDay = 7;
      start.setDate(start.getDate() - currentDay + 1);
      end.setDate(start.getDate() + 6);
    } else if (timeframe === 'month') {
      start.setDate(1);
      end.setFullYear(start.getFullYear(), start.getMonth() + 1, 0);
    } else if (timeframe === 'year') {
      start.setMonth(0, 1);
      end.setFullYear(start.getFullYear(), 11, 31);
    }
    return { start, end };
  }, [timeframe, referenceDate]);

  const isCurrentPeriod = useMemo(() => {
    const now = new Date();
    return now >= timeRange.start && now <= timeRange.end;
  }, [timeRange]);

  const canGoForward = useMemo(() => {
    const nextStart = new Date(timeRange.end);
    nextStart.setMilliseconds(nextStart.getMilliseconds() + 1);
    return nextStart <= new Date();
  }, [timeRange]);

  const canGoBackward = useMemo(() => {
    if (!oldestDate) return true;
    return timeRange.start > oldestDate;
  }, [timeRange, oldestDate]);

  const shiftPeriod = (direction: -1 | 1) => {
    setReferenceDate((prev) => {
      const d = new Date(prev);
      if (timeframe === 'day') d.setDate(d.getDate() + direction);
      else if (timeframe === 'week') d.setDate(d.getDate() + direction * 7);
      else if (timeframe === 'month') d.setMonth(d.getMonth() + direction);
      else if (timeframe === 'year') d.setFullYear(d.getFullYear() + direction);
      return d;
    });
  };

  // Data fetching effect
  useEffect(() => {
    const doFetch = async () => {
      setLoading(true);
      const { data: readingsData } = await supabase
        .from('room_readings')
        .select('*')
        .gte('created_at', timeRange.start.toISOString())
        .lte('created_at', timeRange.end.toISOString())
        .order('created_at', { ascending: true })
        .limit(1000);
      if (readingsData) {
        setReadings(readingsData);
        if (readingsData.length > 0) setLatest(readingsData[readingsData.length - 1]);
      }
      setLoading(false);

      const { data: logsData } = await supabase
        .from('device_logs')
        .select('*')
        .gte('created_at', timeRange.start.toISOString())
        .lte('created_at', timeRange.end.toISOString())
        .order('created_at', { ascending: false })
        .limit(200);
      if (logsData) setLogs(logsData);

      if (activeTab === 'power') {
        const { data: sessionsData } = await supabase
          .from('device_sessions')
          .select('*')
          .order('synced_at', { ascending: false })
          .limit(5000);
        if (sessionsData) setSessions(sessionsData);
      }
    };
    doFetch();
  }, [timeRange, activeTab]);

  // Init: oldest date + realtime subscriptions
  useEffect(() => {
    async function fetchOldestDate() {
      const { data } = await supabase
        .from('room_readings')
        .select('created_at')
        .order('created_at', { ascending: true })
        .limit(1);
      if (data && data.length > 0) setOldestDate(new Date(data[0].created_at));
    }
    fetchOldestDate();

    const channel = supabase
      .channel('live_updates')
      .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'room_readings' }, (payload) => {
        const newReading = payload.new as Reading;
        setReadings((prev) => [...prev.slice(-999), newReading]);
        setLatest(newReading);
      })
      .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'device_logs' }, (payload) => {
        setLogs((prev) => [payload.new as DeviceLog, ...prev].slice(0, 50));
      })
      .subscribe((status) => {
        if (status === 'SUBSCRIBED') setRealtimeStatus('online');
        else if (status === 'CLOSED' || status === 'CHANNEL_ERROR') setRealtimeStatus('offline');
      });

    return () => {
      supabase.removeChannel(channel);
    };
  }, []);

  // Derived data
  const formatSGTime = useCallback((dateStr: string) => {
    return new Date(dateStr).toLocaleString('en-SG', {
      timeZone: 'Asia/Singapore',
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });
  }, []);

  const averages = useMemo(() => {
    if (readings.length === 0) return { temp: 0, hum: 0, lux: 0 };
    // lux_value can be null/undefined on rows written before the lux_value
    // column migration (or before firmware upgrade) -- guard with `|| 0` so
    // a handful of pre-migration rows don't NaN-poison the whole average.
    const sum = readings.reduce(
      (acc, r) => ({
        temp: acc.temp + r.temperature,
        hum: acc.hum + r.humidity,
        lux: acc.lux + (r.lux_value || 0),
      }),
      { temp: 0, hum: 0, lux: 0 }
    );
    return {
      temp: sum.temp / readings.length,
      hum: sum.hum / readings.length,
      lux: sum.lux / readings.length,
    };
  }, [readings]);

  const displayData =
    timeframe === 'day'
      ? {
          temp: latest?.temperature || 0,
          hum: latest?.humidity || 0,
          lux: latest?.lux_value || 0,
          label: 'Latest sync',
        }
      : {
          temp: averages.temp,
          hum: averages.hum,
          lux: averages.lux,
          label: `Period average (${timeframe})`,
        };

  const comfortScore = useMemo(() => {
    if (!latest) return 0;
    const t = latest.temperature;
    const h = latest.humidity;
    let tScore = 100 - Math.abs(t - 24) * 5;
    let hScore = 100 - Math.abs(h - 50) * 1.5;
    tScore = Math.max(0, Math.min(100, tScore));
    hScore = Math.max(0, Math.min(100, hScore));
    return Math.round((tScore + hScore) / 2);
  }, [latest]);

  // NOTE: this is a rough environmental-change heuristic, NOT a reliable
  // presence/occupancy signal -- this device has no dedicated motion/PIR
  // sensor. Light level swings just as easily from daylight, curtains, or
  // automatic lighting as from a person flicking a switch, and humidity
  // drifts from many causes unrelated to anyone entering/leaving. Labeled
  // and framed accordingly below (as raw sensor shifts, not "user activity").
  const environmentEvents = useMemo(() => {
    const events: { time: string; label: 'Dimmer / Drier' | 'Brighter / Humid' }[] = [];
    if (readings.length < 2) return events;
    // Compare each reading to the previous one (not a fixed time window) so this
    // works regardless of the device's sleep interval (5/15/30/60 min) — a 30-min
    // rolling window with a "need 5+ samples" gate almost never fires once the
    // interval exceeds ~5 minutes.
    // Intentionally uses ldr_value (raw ADC), NOT lux_value: this is an
    // internal heuristic already tuned against raw ADC behavior, not a
    // human-facing display value. Re-deriving it against the nonlinear
    // raw->lux conversion would require re-tuning the 0.4 relative-change
    // threshold below for no real benefit.
    // LDR polarity: bright room = LOW raw reading, dark room = HIGH raw
    // reading (firmware's ADC-to-lux conversion inverts this back to the
    // intuitive "higher lux = brighter" sense, but this heuristic works
    // directly on the raw, uninverted signal). So a "dimmer/drier" shift
    // pushes raw ldr_value UP; a "brighter/humid" shift pushes it DOWN.
    let currentState: 'BrighterHumid' | 'DimmerDrier' = 'BrighterHumid';
    for (let i = 1; i < readings.length; i++) {
      const past = readings[i - 1];
      const current = readings[i];
      const ldrChange = (current.ldr_value - past.ldr_value) / Math.max(1, past.ldr_value);
      const humChange = current.humidity - past.humidity;
      if ((ldrChange >= 0.4 || humChange <= -3) && currentState !== 'DimmerDrier') {
        events.push({ time: current.created_at, label: 'Dimmer / Drier' });
        currentState = 'DimmerDrier';
      } else if ((ldrChange <= -0.4 || humChange >= 3) && currentState !== 'BrighterHumid') {
        events.push({ time: current.created_at, label: 'Brighter / Humid' });
        currentState = 'BrighterHumid';
      }
    }
    return events.reverse().slice(0, 5);
  }, [readings]);

  const lastUpdateStr = latest
    ? new Date(latest.created_at).toLocaleTimeString([], {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false,
      })
    : '--:--:--';

  const filteredSessions = useMemo(() => {
    return sessions.filter((s) => {
      const d = new Date(s.synced_at);
      return d >= timeRange.start && d <= timeRange.end;
    });
  }, [sessions, timeRange]);

  // Period navigation UI
  const periodLabel = useMemo(() => {
    const start = timeRange.start;
    if (timeframe === 'day') {
      if (isCurrentPeriod) return 'Today';
      return start.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', day: 'numeric', year: 'numeric' });
    } else if (timeframe === 'week') {
      const end = timeRange.end;
      if (isCurrentPeriod) return 'This week';
      return `${start.toLocaleDateString('en-SG', { month: 'short', day: 'numeric' })} – ${end.toLocaleDateString('en-SG', { month: 'short', day: 'numeric', year: 'numeric' })}`;
    } else if (timeframe === 'month') {
      if (isCurrentPeriod) return 'This month';
      return start.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', month: 'short', year: 'numeric' });
    } else {
      if (isCurrentPeriod) return 'This year';
      return start.toLocaleDateString('en-SG', { timeZone: 'Asia/Singapore', year: 'numeric' });
    }
  }, [timeRange, timeframe, isCurrentPeriod]);

  const periodNavigation = (
    <div className="flex flex-wrap items-center gap-2">
      <div className="flex bg-[#16161a] p-1 rounded-lg border border-[#1f1f23] gap-1">
        <button
          onClick={() => shiftPeriod(-1)}
          disabled={!canGoBackward}
          className={`px-2 py-1 flex items-center rounded transition-colors ${
            !canGoBackward ? 'text-[#3f3f46] cursor-not-allowed' : 'text-[#a1a1aa] hover:bg-[#1f1f23]'
          }`}
        >
          <span className="material-symbols-outlined text-[16px]">chevron_left</span>
        </button>
        <div className="relative flex items-center justify-center min-w-[100px] md:min-w-[130px]">
          <button
            onClick={() => setIsDatePickerOpen(!isDatePickerOpen)}
            className="flex items-center gap-1 text-[#a1a1aa] hover:text-[#f4f4f5] transition-colors"
          >
            <span className="material-symbols-outlined text-[14px]">calendar_today</span>
            <span className="text-xs">{periodLabel}</span>
          </button>
          {isDatePickerOpen && (
            <DatePicker
              mode={timeframe}
              selectedDate={referenceDate}
              minDate={oldestDate}
              onSelect={(d) => {
                setReferenceDate(d);
                setIsDatePickerOpen(false);
              }}
              onClose={() => setIsDatePickerOpen(false)}
            />
          )}
        </div>
        <button
          onClick={() => shiftPeriod(1)}
          disabled={!canGoForward}
          className={`px-2 py-1 flex items-center rounded transition-colors ${
            !canGoForward ? 'text-[#3f3f46] cursor-not-allowed' : 'text-[#a1a1aa] hover:bg-[#1f1f23]'
          }`}
        >
          <span className="material-symbols-outlined text-[16px]">chevron_right</span>
        </button>
      </div>
      <div className="flex bg-[#16161a] p-1 rounded-lg border border-[#1f1f23] gap-1">
        {(['day', 'week', 'month', 'year'] as Timeframe[]).map((tf) => (
          <button
            key={tf}
            onClick={() => { setTimeframe(tf); setReferenceDate(new Date()); }}
            className={`px-3 py-1 text-xs rounded transition-colors ${
              timeframe === tf ? 'bg-[#818cf8] text-[#0d0d0f] font-medium' : 'text-[#a1a1aa] hover:bg-[#1f1f23]'
            }`}
          >
            {tf === 'day' ? 'Day' : tf === 'week' ? 'Week' : tf === 'month' ? 'Month' : 'Year'}
          </button>
        ))}
      </div>
    </div>
  );

  return (
    <Layout realtimeStatus={realtimeStatus} activeTab={activeTab} onTabChange={setActiveTab}>
      {activeTab === 'dashboard' && (
        <div className="flex flex-col gap-6">
          <div className="flex items-center gap-4">
            <div>
              <p className="text-xs text-[#6b7280]">System monitoring</p>
              <div className="flex flex-wrap items-center gap-2 sm:gap-3">
                <h1 className="text-xl sm:text-2xl font-semibold text-[#f4f4f5]">Environmental Overview</h1>
                <div className="px-2 py-0.5 bg-[#16161a] border border-[#1f1f23] rounded text-[11px] text-[#6b7280] font-mono">
                  Sync {lastUpdateStr}
                </div>
              </div>
            </div>
          </div>
          <DashboardView
            readings={readings}
            latest={latest}
            logs={logs}
            timeframe={timeframe}
            timeRange={timeRange}
            loading={loading}
            environmentEvents={environmentEvents}
            formatSGTime={formatSGTime}
            comfortScore={comfortScore}
            displayData={displayData}
            periodNavigation={periodNavigation}
          />
        </div>
      )}

      {activeTab === 'power' && (
        <PowerView
          sessions={sessions}
          filteredSessions={filteredSessions}
          periodNavigation={periodNavigation}
        />
      )}
    </Layout>
  );
}
