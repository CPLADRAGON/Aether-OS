-- AETHER_OS: Unified Supabase Schema
-- This script initializes the three core tables required for Aether monitoring.

-- 1. ROOM_READINGS (Environmental Telemetry)
CREATE TABLE IF NOT EXISTS room_readings (
  id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  temperature FLOAT NOT NULL,
  humidity FLOAT NOT NULL,
  ldr_value FLOAT,
  accel_total FLOAT,
  battery_v FLOAT,
  trigger_source TEXT DEFAULT 'auto',
  device_id TEXT DEFAULT 'esp32_01'
);

-- 2. DEVICE_SESSIONS (Runtime & Lifecycle Tracking)
CREATE TABLE IF NOT EXISTS device_sessions (
  id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
  synced_at TIMESTAMPTZ DEFAULT NOW(),
  start_time BIGINT NOT NULL, -- Unix Epoch (UTC)
  duration INTEGER NOT NULL,  -- Duration in seconds
  boot_count INTEGER,
  measure_count INTEGER,
  sleep_interval INTEGER DEFAULT 5, -- Saved sleep profile (minutes)
  device_id TEXT DEFAULT 'esp32_01'
);

-- 3. DEVICE_LOGS (System Events & Debugging)
CREATE TABLE IF NOT EXISTS device_logs (
  id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  message TEXT NOT NULL,
  level TEXT DEFAULT 'INFO', -- DEBUG, INFO, WARN, ERROR
  device_id TEXT DEFAULT 'esp32_01'
);

-- --- SECURITY CONFIGURATION (Row Level Security) ---

ALTER TABLE room_readings ENABLE ROW LEVEL SECURITY;
ALTER TABLE device_sessions ENABLE ROW LEVEL SECURITY;
ALTER TABLE device_logs ENABLE ROW LEVEL SECURITY;

-- Anonymous Insert Policies (For ESP32)
CREATE POLICY "Allow anon insert readings" ON room_readings FOR INSERT WITH CHECK (true);
CREATE POLICY "Allow anon insert sessions" ON device_sessions FOR INSERT WITH CHECK (true);
CREATE POLICY "Allow anon insert logs" ON device_logs FOR INSERT WITH CHECK (true);

-- Anonymous Select Policies (For Dashboard & Bot)
CREATE POLICY "Allow anon select readings" ON room_readings FOR SELECT USING (true);
CREATE POLICY "Allow anon select sessions" ON device_sessions FOR SELECT USING (true);
CREATE POLICY "Allow anon select logs" ON device_logs FOR SELECT USING (true);
