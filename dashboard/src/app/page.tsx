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
    if (readings.length === 0) return { temp: 0, hum: 0, ldr: 0 };
    const sum = readings.reduce(
      (acc, r) => ({
        temp: acc.temp + r.temperature,
        hum: acc.hum + r.humidity,
        ldr: acc.ldr + r.ldr_value,
      }),
      { temp: 0, hum: 0, ldr: 0 }
    );
    return {
      temp: sum.temp / readings.length,
      hum: sum.hum / readings.length,
      ldr: sum.ldr / readings.length,
    };
  }, [readings]);

  const displayData =
    timeframe === 'day'
      ? {
          temp: latest?.temperature || 0,
          hum: latest?.humidity || 0,
          ldr: latest?.ldr_value || 0,
          label: 'Latest sync',
        }
      : {
          temp: averages.temp,
          hum: averages.hum,
          ldr: averages.ldr,
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

  const presenceEvents = useMemo(() => {
    const events: { time: string; label: 'User Out' | 'User Home' }[] = [];
    if (readings.length < 2) return events;
    const thirtyMinsAgo = new Date();
    thirtyMinsAgo.setMinutes(thirtyMinsAgo.getMinutes() - 30);
    const recent = readings.filter((r) => new Date(r.created_at) >= thirtyMinsAgo);
    if (recent.length < 5) return events;
    let currentState = 'Home';
    for (let i = 5; i < recent.length; i++) {
      const past = recent[i - 5];
      const current = recent[i];
      const ldrChange = (current.ldr_value - past.ldr_value) / Math.max(1, past.ldr_value);
      const humChange = current.humidity - past.humidity;
      if (ldrChange <= -0.5 && humChange <= -0.5 && currentState !== 'Out') {
        events.push({ time: current.created_at, label: 'User Out' });
        currentState = 'Out';
      } else if (ldrChange >= 0.5 && humChange >= 0.5 && currentState !== 'Home') {
        events.push({ time: current.created_at, label: 'User Home' });
        currentState = 'Home';
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
    <div className="flex items-center gap-2">
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
              <div className="flex items-center gap-3">
                <h1 className="text-2xl font-semibold text-[#f4f4f5]">Environmental Overview</h1>
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
            presenceEvents={presenceEvents}
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
