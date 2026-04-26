# Design Spec: Advanced Features (LED Toggle, Runtime Batching, & Telegram Bot)

Date: 2026-04-26
Topic: Firmware & Dashboard Enhancements
Status: Draft

## 1. Goal Description
Implement three major features to enhance the ESP32 Room Monitor (Aether):
1.  **LED Control**: A new menu item in the physical OLED interface to toggle the RGB LED (On/Off) via long-press.
2.  **Runtime Analytics**: A high-performance batching system to track every device session's duration and sync it to the cloud without extra battery penalty.
3.  **Telegram Bot (Aether)**: A cute cartoon companion for remote status checks, dashboard access, and automated reports.
4.  **Performance Refactor**: Move from JSON-based NVS storage to packed binary structs for "Zero-Wait" boot performance.

---

## 2. Component Design

### 2.1 Firmware: LED Toggle & Stealth Mode
- **UI Change**: Add `PAGE_LED` to the `MenuPage` enum.
- **Label**: "LED: ON" or "LED: OFF".
- **Logic**: 
    - Selecting this item (long-press) toggles a global `ledEnabled` flag in `DeviceConfig` struct.
    - **Stealth Mode**: Read `ledEnabled` at the very beginning of `setup()`. If false, pins are initialized to high-impedance (INPUT) to draw zero current.
- **Persistence**: Save to NVS via `DeviceConfig` binary blob.

### 2.2 Firmware: Binary NVS & Runtime Batching
- **Binary Structs**:
    - `WiFiSnapshot`: Versioned blob for instant connection.
    - `DeviceConfig`: Stores `ledEnabled` and other settings.
    - `LogQueue`: Array of up to 10 `SessionLog` entries (`startTime`, `duration`).
- **Tracking**:
    - Record `sessionStartTime` (UTC if synced, else relative) at boot.
    - Append to `LogQueue` before deep sleep.
- **Sync Logic**:
    - Upload entire `LogQueue` + current `bootCount`/`measureCount` in one batch.
    - Clear queue only after HTTP 200 OK.

### 2.3 Backend: Supabase Schema
- **New Table**: `device_sessions`
    - `id`: Primary Key
    - `created_at`: Timestamp (when session started)
    - `duration`: Integer (seconds)
    - `boot_count`: Integer
    - `measure_count`: Integer
    - `device_id`: Text (default 'esp32_01')

### 2.4 Dashboard: Runtime & Stats Page
- **URL**: `/runtime`
- **Features**:
    - Charts showing Uptime vs Boots over time.
    - Syncs `boot_count` and `measure_count` to display lifetime stats on the web.
    - Bar charts for session distribution.

### 2.5 Telegram Bot: Aether
- **Identity**:
    - **Name**: Aether Monitor Bot
    - **Mascot**: Futuristic cyan-accented AI (see generated image).
- **Backend**: Next.js API route (`/api/telegram/webhook`).
- **Functions**:
    - `/url`: Returns the dashboard link.
    - `/report`: Generates a summary (Temp/Hum/Uptime) for a specified period.
    - `/status`: Real-time snapshot of the latest reading.
    - `/stats`: Shows accumulated lifetime statistics (Total Runtime, Boot Count, Measurement Count).
    - **Alerts**: Push notifications for high/low environmental thresholds.

---

## 3. Visual Concept: Aether Bot
![Cute Aether Bot Mascot](file:///C:/Users/WANG/.gemini/antigravity/brain/197cbaa3-5a21-46d6-a7c7-9db58fc8d388/aether_bot_cute_mascot_1777178614059.png)

---

## 4. Proposed Approaches & Trade-offs

| Approach | Performance | Granularity | Implementation Effort |
| :--- | :--- | :--- | :--- |
| **Batch Log (Selected)** | **High** (Single sync) | **High** (Per-session logs) | Medium (NVS management) |
| Extra Column | Very High | Low (Measurement-only) | Low |
| Real-time Sync | Low (Double handshake) | High | Medium |

---

## 5. Success Criteria
- [ ] User can toggle LED via the OLED menu.
- [ ] "Total Uptime" in firmware stats reflects accumulated lifetime time.
- [ ] Dashboard shows a history of session durations.
- [ ] Telegram bot responds to commands with current data.
