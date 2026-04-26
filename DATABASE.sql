-- Create the device_sessions table for runtime tracking
CREATE TABLE device_sessions (
  id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
  created_at TIMESTAMPTZ DEFAULT NOW(),
  duration INTEGER NOT NULL, -- duration in seconds
  boot_count INTEGER,
  measure_count INTEGER,
  device_id TEXT DEFAULT 'esp32_01'
);

-- Enable Row Level Security (RLS)
ALTER TABLE device_sessions ENABLE ROW LEVEL SECURITY;

-- Allow anonymous inserts (for the ESP32)
CREATE POLICY "Allow anon insert sessions" ON device_sessions
FOR INSERT WITH CHECK (true);

-- Allow anonymous selects (for the dashboard and bot)
CREATE POLICY "Allow anon select sessions" ON device_sessions
FOR SELECT USING (true);
