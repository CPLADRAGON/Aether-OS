#include "secrets.h"
#include "display_manager.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <driver/rtc_io.h>
#include <math.h>
#include <time.h>

// --- RTC Persistent Memory ---
int bootCount = 0;
int measureCount = 0;

// Dynamic Location Data (Persistent in NVS)
bool locationSynced = false;
float locLat = 0.0;
float locLon = 0.0;
long locOffset = 28800;
char locCity[16] = "UNKNOWN";

Preferences preferences;
SemaphoreHandle_t displayMutex;
WebServer server(80);
DNSServer dnsServer;

enum SystemState {
  SS_MENU,
  SS_CONNECTING,
  SS_SCANNING,
  SS_SYNCING,
  SS_LOCATING,
  SS_CLOCK,
  SS_WEATHER,
  SS_SLEEPING,
  SS_STATS,
  SS_RESET,
  SS_PORTAL,
  SS_WIFI_MENU
};
volatile SystemState currentState = SS_MENU;
String uiLine1 = "", uiLine2 = "", uiLine3 = "";
dm::Icon uiIcon = dm::ICON_WIFI;
bool uiHasIcon = false;

// --- Binary Structs (NVS Optimization) ---
struct __attribute__((packed)) WiFiSnapshot {
  uint8_t version = 1;
  char ssid[33];
  uint8_t bssid[6];
  uint8_t channel;
  uint32_t ip;
  uint32_t gateway;
  uint32_t subnet;
  uint32_t dns;
  uint32_t crc;
};

struct __attribute__((packed)) DeviceConfig {
  uint8_t version = 1;
  bool ledEnabled = true;
  int primSlot = 1;
  bool locationSynced = false;
  float lat = 0.0;
  float lon = 0.0;
  long offset = 28800;
  char city[16];
  uint32_t totalLifetimeRuntime = 0; // Total seconds
  uint16_t sleepMinutes = 5;         // NEW: Default 5 min
};

struct __attribute__((packed)) SessionLog {
  uint32_t startTime; // UTC Epoch
  uint16_t duration;  // Seconds
};

struct __attribute__((packed)) LogQueue {
  uint8_t version = 1;
  uint8_t count = 0;
  SessionLog logs[10];
};

// --- Local Trend History (ROOM/TREND menu features) ---
// Ring buffer of the last 12 completed MEASURE averages. `head` is the index
// of the most-recently-written point (not "next write index"); this keeps
// the read-side chronological-order math simple and unambiguous.
struct __attribute__((packed)) TrendPoint {
  int16_t  tempX10; // temperature * 10, e.g. 235 = 23.5C
  uint8_t  humidity; // 0-100
  uint16_t ldr;      // raw analogRead(LDR_PIN), 0-4095
};

struct __attribute__((packed)) TrendHistory {
  uint8_t version = 1;
  uint8_t count = 0; // valid entries, 0-12
  uint8_t head = 0;  // index of the most recently written point
  TrendPoint points[12];
};

WiFiSnapshot currentSnapshot;
DeviceConfig config;
LogQueue logQueue;
TrendHistory trendHistory;
uint32_t sessionStartTime = 0; // Relative (millis) or Absolute (UTC)
bool timeSynced = false;

uint32_t calculateCRC(WiFiSnapshot *s) {
  uint32_t originalCrc = s->crc;
  s->crc = 0;
  uint32_t crc = 0;
  uint8_t *p = (uint8_t *)s;
  for (int i = 0; i < sizeof(WiFiSnapshot); i++)
    crc += p[i];
  s->crc = originalCrc;
  return crc;
}

void saveWiFiSnapshot() {
  currentSnapshot.version = 1;
  strncpy(currentSnapshot.ssid, WiFi.SSID().c_str(), 32);
  memcpy(currentSnapshot.bssid, WiFi.BSSID(), 6);
  currentSnapshot.channel = WiFi.channel();
  currentSnapshot.ip = (uint32_t)WiFi.localIP();
  currentSnapshot.gateway = (uint32_t)WiFi.gatewayIP();
  currentSnapshot.subnet = (uint32_t)WiFi.subnetMask();
  currentSnapshot.dns = (uint32_t)WiFi.dnsIP();
  currentSnapshot.crc = calculateCRC(&currentSnapshot);

  preferences.begin("wifi_v2", false);
  preferences.putBytes("snap", &currentSnapshot, sizeof(WiFiSnapshot));
  preferences.end();
  Serial.println("[NVS] Binary snapshot saved.");
}

bool loadWiFiSnapshot() {
  preferences.begin("wifi_v2", true);
  size_t len =
      preferences.getBytes("snap", &currentSnapshot, sizeof(WiFiSnapshot));
  preferences.end();
  if (len != sizeof(WiFiSnapshot))
    return false;
  return (currentSnapshot.crc == calculateCRC(&currentSnapshot));
}

void saveConfig() {
  preferences.begin("config_v2", false);
  preferences.putBytes("data", &config, sizeof(DeviceConfig));
  preferences.end();
}

void loadConfig() {
  preferences.begin("config_v2", true);
  size_t len = preferences.getBytes("data", &config, sizeof(DeviceConfig));
  preferences.end();
  if (len != sizeof(DeviceConfig)) {
    // Default values for new installation
    config.version = 1;
    config.ledEnabled = true;
    config.primSlot = 1;
    config.locationSynced = false;
    config.sleepMinutes = 5;
    config.totalLifetimeRuntime = 0;
    saveConfig();
    return;
  }

  // Optional: Handle migration if structure changes but version stays 1
  if (config.sleepMinutes == 0) {
    config.sleepMinutes = 5;
    saveConfig();
  }
}

void appendSessionLog(uint32_t start, uint16_t dur) {
  preferences.begin("logs_v2", false);
  preferences.getBytes("queue", &logQueue, sizeof(LogQueue));
  if (logQueue.count < 10) {
    logQueue.logs[logQueue.count] = {start, dur};
    logQueue.count++;
  } else {
    // Shift left to make room (FIFO)
    for (int i = 0; i < 9; i++)
      logQueue.logs[i] = logQueue.logs[i + 1];
    logQueue.logs[9] = {start, dur};
  }
  preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
  preferences.end();
  Serial.printf("[NVS] Log added: %ds. Total in queue: %d\n", dur,
                logQueue.count);
}

void clearLogQueue() {
  preferences.begin("logs_v2", false);
  logQueue.count = 0;
  preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
  preferences.end();
  Serial.println("[NVS] Log queue cleared.");
}

// Loads trendHistory from NVS at boot. If the stored blob is missing, the
// wrong size, or a different version, resets to an empty (count=0) history
// rather than risk reading garbage data into the ring buffer.
void loadTrendHistory() {
  preferences.begin("trend_v1", true);
  size_t len = preferences.getBytes("hist", &trendHistory, sizeof(TrendHistory));
  preferences.end();
  if (len != sizeof(TrendHistory) || trendHistory.version != 1) {
    trendHistory.version = 1;
    trendHistory.count = 0;
    trendHistory.head = 0;
    return;
  }
  // Defensive range check: a torn/partial flash write could otherwise leave
  // count/head outside their valid ranges, which would cause an out-of-bounds
  // stack write in any code that loops `for (i = 0; i < count; i++)` over a
  // fixed-size points[12] array (e.g. drawTrendSparkline()).
  if (trendHistory.count > 12 || trendHistory.head > 11) {
    trendHistory.count = 0;
    trendHistory.head = 0;
  }
}

// Appends one averaged MEASURE reading to the local trend ring buffer and
// persists it immediately. Called regardless of WiFi/upload outcome — this
// is local-only data, independent of connectivity. Non-fatal on NVS failure:
// the core measure/upload path must never be blocked by this.
void appendTrendPoint(float tempC, float humidityPct, int ldrRaw) {
  int writeIdx = (trendHistory.count == 0) ? 0 : (trendHistory.head + 1) % 12;

  int tempTenths = (int)(tempC * 10.0f + (tempC >= 0 ? 0.5f : -0.5f));
  if (tempTenths > 32767) tempTenths = 32767;
  if (tempTenths < -32768) tempTenths = -32768;

  int humInt = (int)(humidityPct + 0.5f);
  if (humInt < 0) humInt = 0;
  if (humInt > 100) humInt = 100;

  trendHistory.points[writeIdx].tempX10 = (int16_t)tempTenths;
  trendHistory.points[writeIdx].humidity = (uint8_t)humInt;
  trendHistory.points[writeIdx].ldr = (uint16_t)ldrRaw;
  trendHistory.head = (uint8_t)writeIdx;
  if (trendHistory.count < 12) trendHistory.count++;

  preferences.begin("trend_v1", false);
  preferences.putBytes("hist", &trendHistory, sizeof(TrendHistory));
  preferences.end();
  Serial.printf("[TREND] Point appended. count=%d head=%d\n",
                trendHistory.count, trendHistory.head);
}

void migrateNVS() {
  preferences.begin("aether", true);
  if (preferences.isKey("locSync")) {
    Serial.println("[NVS] Legacy data found. Migrating...");
    config.locationSynced = preferences.getBool("locSync", false);
    config.lat = preferences.getFloat("lat", 0.0);
    config.lon = preferences.getFloat("lon", 0.0);
    config.offset = preferences.getLong("offset", 28800);
    String city = preferences.getString("city", "UNKNOWN");
    strncpy(config.city, city.c_str(), 15);
    config.primSlot = preferences.getInt("primSlot", 1);
    preferences.end();

    saveConfig();

    // Clear old keys to prevent re-migration
    preferences.begin("aether", false);
    preferences.remove("locSync");
    preferences.remove("lat");
    preferences.remove("lon");
    preferences.remove("offset");
    preferences.remove("city");
    preferences.end();
    Serial.println("[NVS] Migration complete.");
  } else {
    preferences.end();
  }
}

// --- Pin Definitions ---
#define DHTPIN 4
#define DHTTYPE DHT11
#define BUTTON_PIN 33
#define LDR_PIN 34
#define RED_PIN 13
#define GREEN_PIN 14
#define BLUE_PIN 27
#define OLED_RST 16

// ROOM page comfort/light thresholds. These are starting values, not
// calibrated to any specific sensor unit — tune against your actual DHT11
// and LDR during the Task 8 hardware verification pass.
#define ROOM_TEMP_COLD_MAX  18   // <18C  = COLD
#define ROOM_TEMP_COOL_MAX  23   // <23C  = COOL (else WARM below HOT max)
#define ROOM_TEMP_WARM_MAX  28   // <28C  = WARM, >=28C = HOT
#define ROOM_HUM_DRY_MAX    40   // <40%  = DRY
#define ROOM_HUM_NORMAL_MAX 60   // <60%  = NORMAL, >=60% = HUMID
#define ROOM_LDR_BRIGHT_MAX 500  // <500  = BRIGHT (this LDR wiring reads LOW under strong light)
#define ROOM_LDR_DARK_MIN   2000 // >=2000 = DARK, between = DIM

// --- OLED Config (native 64x48 panel via U8g2 ER constructor) ---
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
#define OLED_W 64
#define OLED_H 48
#define OLED_OFFSET_X 0
#define OLED_OFFSET_Y 0
// Backwards-compat: legacy call sites reference these as "icons". Kept as
// dm::Icon aliases so the existing updateOLED(..., icon) signature works.
static const dm::Icon icon_wifi  = dm::ICON_WIFI;
static const dm::Icon icon_cloud = dm::ICON_CLOUD;
static const dm::Icon icon_pin   = dm::ICON_PIN;
static const dm::Icon icon_scan  = dm::ICON_SCAN;

// --- Menu Configuration ---
enum MenuPage {
  PAGE_MEASURE,
  PAGE_TIME,
  PAGE_WEATHER,
  PAGE_LOCATE,
  PAGE_LED,
  PAGE_INTERVAL,
  PAGE_STATS,
  PAGE_ROOM,
  PAGE_TREND,
  PAGE_PORTAL,
  PAGE_RESET,
  PAGE_SLEEP
};
const int TOTAL_MENU_ITEMS = 12;
const char *menuItems[] = {"MEASURE",     "TIME",       "WEATHER",     "LOCATE",
                           "LED",         "INTERVAL",   "STATS",       "ROOM",
                           "TREND",       "WIFI MENU",  "RESET STATS", "DEEP SLEEP"};

enum WiFiMenuPage { WF_PORTAL, WF_SELECT, WF_CLEAR, WF_BACK };
const char *wifiMenuItems[] = {"PORTAL", "SET TARGET", "CLEAR", "BACK"};
const int TOTAL_WIFI_MENU_ITEMS = 4;
int currentWiFiMenuIndex = 0;
int wifiPriority = 0; // 0: Auto (Cycle 5), 1: Fixed (Selected Slot Only)
const char *prioLabels[] = {"AUTO", "FIXED"};

const int VISIBLE_MENU_ITEMS = 3;
int currentMenuIndex = 0;
unsigned long lastInteractionTime = 0;
#define MENU_TIMEOUT 15000
#define LONG_PRESS_MS 1200

// --- Button ISR State ---
volatile bool buttonEvent = false;
volatile bool isPressing = false;
volatile bool longPressTriggered = false;
volatile unsigned long lastButtonEvent = 0;
volatile unsigned long isrPressStart = 0;
volatile unsigned long isrPressDuration = 0;

void IRAM_ATTR handleButtonInterrupt() {
  unsigned long now = millis();
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (now - lastButtonEvent > 200) {
      isrPressStart = now;
      isPressing = true;
      longPressTriggered = false;
    }
  } else {
    if (isPressing) {
      isPressing = false;
      if (!longPressTriggered && (now - isrPressStart > 50)) {
        buttonEvent = true; // Only trigger event for short press if long press
                            // wasn't already triggered
      }
      lastButtonEvent = now;
    }
  }
}

// LEDC Channels
#define RED_CH 0
#define GREEN_CH 1
#define BLUE_CH 2

// --- Global Objects ---
DHT dht(DHTPIN, DHTTYPE);
Adafruit_MPU6050 mpu;
bool mpuFound = false;
bool oledFound = false;

// Forward declarations for the redesigned page renderers (defined near
// showStatsPage(); referenced by runLocatePage/showTimePage/showWeatherPage/
// runMeasurementFlow which come before their definitions).
static void drawClockScreen(const char *hhmm, const char *ss,
                            const char *ddmmyy, const char *day);
static void drawWeatherScreen(float tempC, int humPct, const char *desc);
static void drawStatsScreen(uint32_t measures, uint32_t boots, uint32_t uptimeMin);
static void drawLocateResult(const char *city);
static void drawMeasureSample(int sampleIdx, int totalSamples,
                              float tempC, int humPct, int ldr);
static void drawMeasureGlitch(int sampleIdx, int totalSamples);
// Redesign pass 2 — replaces the last of the generic updateOLED screens.
static void drawErrorScreen(const char *title, const char *code, const char *hint);
static void drawLockedScreen(dm::Icon featureIcon, const char *featureName);
static void drawWifiConnecting(const char *ssid, int elapsedMs);
// Bespoke success/confirmation screen used where dm::toast() can't render
// (blocking subpages hold g_menuOwnedByPage=true, suppressing uiTask overlay).
static void drawConfirmScreen(const char *title, const char *big, const char *hint);
// Confirmation prompt with a "hold to confirm" action; returns true if the
// user long-pressed within timeoutMs.
static bool drawConfirmPromptAndWait(const char *title, const char *body,
                                     const char *hint, uint32_t timeoutMs);

void setLED(int r, int g, int b) {
  if (!config.ledEnabled && (r > 0 || g > 0 || b > 0)) {
    // Only allow brief flash if disabled, or just keep off
    ledcWrite(RED_CH, 0);
    ledcWrite(GREEN_CH, 0);
    ledcWrite(BLUE_CH, 0);
    return;
  }
  ledcWrite(RED_CH, r);
  ledcWrite(GREEN_CH, g);
  ledcWrite(BLUE_CH, b);
}

void toggleLED() {
  config.ledEnabled = !config.ledEnabled;
  saveConfig();
}

void cycleSleepInterval() {
  if (config.sleepMinutes == 5)
    config.sleepMinutes = 15;
  else if (config.sleepMinutes == 15)
    config.sleepMinutes = 30;
  else if (config.sleepMinutes == 30)
    config.sleepMinutes = 60;
  else
    config.sleepMinutes = 5;
  saveConfig();
}

// Helper to wait while checking for ISR button events
bool waitWithButtonPoll(unsigned long ms) {
  buttonEvent = false; // Reset before waiting
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (buttonEvent) {
      buttonEvent = false;
      lastInteractionTime = millis();
      return true;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  return false;
}

// --- Menu scroll tween state (Phase 1) ---
// menuTween interpolates between the previous and next currentMenuIndex when a
// button press advances the menu. drawMenuAnimated() consumes the tween value.
static dm::Tween menuTween;
static float menuScrollPos = 0.f;   // last committed fractional index
static dm::Tween wifiMenuTween;
static float wifiMenuScrollPos = 0.f;

// Shortest-signed delta for circular scroll (e.g., 9 -> 0 in a 10-item ring
// scrolls +1 forward instead of -9 backward).
static float shortestSignedDelta(int from, int to, int ringSize) {
  int d = ((to - from) % ringSize + ringSize) % ringSize;
  if (d > ringSize / 2) d -= ringSize;
  return (float)d;
}

// Page-transition state — see runSlideTransition() in Phase 2 wiring.
static uint8_t g_slideBuf[384];  // 64x48 SSD1306 framebuffer size (6 pages * 64)
static bool    g_slideBufValid = false;

// Icon strip mirroring menuItems / wifiMenuItems for cover-flow rendering.
static const dm::Icon menuIcons[] = {
    dm::ICON_MEASURE_LG, dm::ICON_TIME_LG, dm::ICON_WEATHER_LG, dm::ICON_LOCATE_LG,
    dm::ICON_LED_LG, dm::ICON_INTERVAL_LG, dm::ICON_STATS_LG, dm::ICON_ROOM_LG,
    dm::ICON_TREND_LG, dm::ICON_WIFIMENU_LG, dm::ICON_RESET_LG, dm::ICON_SLEEP_LG
};
static const dm::Icon wifiMenuIcons[] = {
    dm::ICON_PORTAL_LG, dm::ICON_SELECT_LG, dm::ICON_CLEAR_LG, dm::ICON_BACK_LG
};

// Set true by monitorTask right before a menu action; uiTask uses this to
// suppress menu rendering while pages take over.
static volatile bool g_menuOwnedByPage = false;

void drawPulsingPower(int frame) {
  int cx = OLED_OFFSET_X + OLED_W / 2;
  int cy = OLED_OFFSET_Y + 26;
  float pulse = (sin(frame * 0.2) + 1.0) / 2.0;
  int r = 7 + (int)(pulse * 3);
  dm::drawCircle(cx, cy, r);
  dm::clearRect(cx - 3, cy - r - 2, 7, 5);
  dm::drawLine(cx, cy - r + 1, cx, cy - 2);
}

// Renders the main menu using the icon cover-flow layout with the current
// tweened scroll position.
static void renderMainMenu(float fractionalIdx) {
  // Header with WiFi indicator on the right.
  bool wifiUp = (WiFi.status() == WL_CONNECTED);
  String headStr;
  if (locationSynced)
    headStr = String(locCity).substring(0, 6);
  else
    headStr = "AETHER";
  headStr.toUpperCase();
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W,
                 headStr.c_str(), wifiUp, dm::ICON_WIFI);
  if (!wifiUp) {
    dm::setFont(dm::FONT_SMALL);
    dm::drawTextInverted(OLED_OFFSET_X + OLED_W - 6, OLED_OFFSET_Y + 2, "x");
  }

  // Cover-flow: shows current icon centred with prev/next visible at edges.
  dm::drawIconMenu(menuItems, menuIcons, TOTAL_MENU_ITEMS, fractionalIdx);

  // Auto-sleep countdown bar at very bottom
  unsigned long elapsed = millis() - lastInteractionTime;
  if (elapsed < MENU_TIMEOUT) {
    int barWidth = map(elapsed, 0, MENU_TIMEOUT, OLED_W - 4, 0);
    dm::drawFilledRect(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 2,
                       barWidth, 1);
  }
}

static void renderWifiMenu(float fractionalIdx) {
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W,
                 "WIFI CFG", false, dm::ICON_WIFI);
  dm::drawIconMenu(wifiMenuItems, wifiMenuIcons, TOTAL_WIFI_MENU_ITEMS, fractionalIdx);
}

void uiTask(void *pvParameters) {
  static dm::Animation spinnerAnim(150);
  static dm::Animation portalDotAnim(400);
  const char *loader = "/|-\\";
  char spinnerBuf[2] = {0, 0};
  while (1) {
    bool spinnerAdvanced = spinnerAnim.tick();
    bool portalAdvanced = portalDotAnim.tick();
    bool anyAnim = spinnerAdvanced || portalAdvanced;

    // Menu rendering is suppressed while a subpage owns the framebuffer
    // (STATS / CLOCK / WEATHER / LOCATE result / SAVED-WIFI viewer). Other
    // state-driven screens (SS_PORTAL, SS_CONNECTING, SS_SYNCING, ...) MUST
    // still render — those pages rely on uiTask to paint them.
    bool renderMenu = (currentState == SS_MENU || currentState == SS_WIFI_MENU)
                      && !g_menuOwnedByPage;

    if (renderMenu) {
      dm::Tween &tw = (currentState == SS_MENU) ? menuTween : wifiMenuTween;
      float &scroll = (currentState == SS_MENU) ? menuScrollPos : wifiMenuScrollPos;
      if (tw.active()) {
        scroll = tw.value();
        anyAnim = true;
      }

      if (dm::beginFrame(50)) {
        dm::markDirty();
        if (currentState == SS_MENU) {
          renderMainMenu(scroll);
        } else {
          renderWifiMenu(scroll);
        }
        dm::toastTick();
        dm::endFrame();
      }
    } else if (currentState == SS_CONNECTING || currentState == SS_SYNCING ||
               currentState == SS_LOCATING || currentState == SS_SCANNING ||
               currentState == SS_PORTAL) {
      if (dm::beginFrame(50)) {
        // Every progress state animates at ~60fps.
        anyAnim = true;
        dm::markDirty();

        // Header title per state.
        const char *head = "WAITING";
        if (currentState == SS_CONNECTING) head = "LINKING";
        else if (currentState == SS_SYNCING) head = "SYNCING";
        else if (currentState == SS_LOCATING) head = "GEO-IP";
        else if (currentState == SS_SCANNING) head = "SCANNING";
        else if (currentState == SS_PORTAL) head = "PORTAL";
        dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, head, false, dm::ICON_WIFI);

        uint32_t nowMs = millis();
        int cx = OLED_OFFSET_X + 14;
        int cy = OLED_OFFSET_Y + 26;

        if (currentState == SS_PORTAL) {
          // Portal: giant SSID label + IP + animated dots (unchanged design).
          // Portal splash — everything must fit within y=0..47 (48px panel).
          dm::setFont(dm::FONT_SMALL);
          dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 13, "JOIN AP:");
          dm::setFont(dm::FONT_LARGE);
          dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 22, "AETHER");
          dm::setFont(dm::FONT_SMALL);
          // IP at bottom row — full width; no other content on this row.
          dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 40, "192.168.4.1");
          // Animated dots moved to y=34 (above IP line, below SSID text)
          // so they never overlap the IP.
          int dcx = OLED_OFFSET_X + OLED_W - 12;
          int dcy = OLED_OFFSET_Y + 34;
          int phase = (portalDotAnim.frame) % 3;
          for (int i2 = 0; i2 < 3; i2++) {
            int on = (phase + i2) % 3;
            if (on == 0) dm::drawFilledCircle(dcx + i2 * 3, dcy, 1);
            else         dm::drawPixel(dcx + i2 * 3, dcy);
          }
        } else if (currentState == SS_CONNECTING) {
          // Expanding wifi arcs pulsing outward from a centre dot.
          dm::drawFilledCircle(cx, cy, 2);
          int phase = (nowMs / 120) % 12;
          for (int i2 = 0; i2 < 3; i2++) {
            int r = ((phase + i2 * 4) % 12) + 3;
            if (r >= 4 && r <= 12) dm::drawCircle(cx, cy, r);
          }
          dm::setFont(dm::FONT_NORMAL);
          dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 20, uiLine1.c_str());
          dm::setFont(dm::FONT_SMALL);
          dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "[TAP] CANCEL");
        } else if (currentState == SS_SYNCING) {
          // Cloud icon (12x12) + orbiting dot to indicate upload.
          dm::drawIcon(cx - 6, cy - 6, dm::ICON_CLOUD);
          float ang = (float)(nowMs) * 0.006f;
          int ox = cx + (int)(cosf(ang) * 10);
          int oy = cy + (int)(sinf(ang) * 10);
          dm::drawFilledCircle(ox, oy, 1);
          dm::setFont(dm::FONT_NORMAL);
          String l1 = (uiLine1 == "") ? String("CLOUD") : uiLine1;
          dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 20, l1.c_str());
          if (uiLine2 != "") {
            dm::setFont(dm::FONT_SMALL);
            dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 32, uiLine2.c_str());
          }
        } else if (currentState == SS_LOCATING) {
          // Pin icon dropping (bounces on Y axis)
          float t = (float)(nowMs % 900) / 900.f;
          int drop = (int)(sinf(t * 3.14159f) * 4);
          dm::drawIcon(cx - 6, cy - 6 - drop, dm::ICON_PIN);
          dm::drawHLine(cx - 8, cy + 8, 16);   // ground line
          dm::setFont(dm::FONT_NORMAL);
          dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 20, "GEO-IP");
          dm::setFont(dm::FONT_SMALL);
          dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "IP-API.COM");
        } else if (currentState == SS_SCANNING) {
          // WiFi scan radar sweep.
          dm::drawCircle(cx, cy, 10);
          float ang = (float)(nowMs) * 0.008f;
          int ex = cx + (int)(cosf(ang) * 10);
          int ey = cy + (int)(sinf(ang) * 10);
          dm::drawLine(cx, cy, ex, ey);
          dm::setFont(dm::FONT_NORMAL);
          dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 20, "SCAN");
        }
        dm::toastTick();
        dm::endFrame();
      }
    }
    // Adaptive frame budget: 16ms during animations, 50ms idle.
    vTaskDelay((anyAnim ? 16 : 50) / portTICK_PERIOD_MS);
  }
}

void sendLog(String msg, String level = "INFO") {
  if (WiFi.status() != WL_CONNECTED)
    return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/device_logs";
  if (http.begin(client, url)) {
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["message"] = msg;
    doc["level"] = level;
    String body;
    serializeJson(doc, body);
    http.POST(body);
    http.end();
  }
}

void updateOLED(String header, String line1, String line2, String line3 = "",
                dm::Icon icon = dm::ICON_WIFI, bool showIcon = false) {
  if (!oledFound)
    return;
  uiLine1 = line1;
  uiLine2 = line2;
  uiLine3 = line3;
  uiIcon = icon;
  uiHasIcon = showIcon;

  dm::showStatus(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W,
                 header.c_str(), line1.c_str(), line2.c_str(),
                 line3.length() ? line3.c_str() : nullptr,
                 showIcon, icon);
}

bool validateIPReady() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  IPAddress out;
  // Aggressive 750ms DNS probe on Supabase host
  uint32_t start = millis();
  while (millis() - start < 750) {
    if (WiFi.hostByName("tsawczfmlqrmtyojkkne.supabase.co", out))
      return true;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
  return false;
}

// Return codes: 0=OK, 1=UserAbort, 2=NoSSID, 3=AuthFail, 4=Timeout
int tryConnect(const char *ssid, const char *pass) {
  String sStr = String(ssid);
  sStr.trim();
  String pStr = String(pass);
  pStr.trim();
  if (sStr.length() == 0)
    return 4;

  // currentState is SS_CONNECTING here (set by ensureWiFi); uiTask draws the
  // expanding-arc wifi connecting splash based on that state.
  uiLine1 = sStr.substring(0, 9);

  WiFi.disconnect();
  vTaskDelay(200 / portTICK_PERIOD_MS);

  bool snapshotValid = loadWiFiSnapshot();
  bool isFastTrackSSID =
      (snapshotValid && sStr == String(currentSnapshot.ssid));
  bool attemptFast = isFastTrackSSID;

  if (attemptFast) {
    WiFi.config(
        IPAddress(currentSnapshot.ip), IPAddress(currentSnapshot.gateway),
        IPAddress(currentSnapshot.subnet), IPAddress(currentSnapshot.dns));
    WiFi.begin(sStr.c_str(), pStr.c_str(), currentSnapshot.channel,
               currentSnapshot.bssid);
  } else {
    WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0),
                IPAddress(0, 0, 0, 0));
    WiFi.begin(sStr.c_str(), pStr.c_str());
  }

  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.printf("[WIFI] Target: %s\n", sStr.c_str());
  uiLine2 = sStr.substring(0, 10);

  for (int i = 0; i < 100; i++) {
    if (buttonEvent) {
      buttonEvent = false;
      Serial.println("[WIFI] Aborted.");
      return 1;
    }

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      Serial.printf("[WIFI] Linked in %dms!\n", i * 100);
      // Save for Memory-Link (Fast-Reconnect)
      preferences.begin("aether", false);
      preferences.putString("lastS", sStr);
      preferences.putString("lastP", pStr);
      preferences.end();
      saveWiFiSnapshot();
      return 0;
    }

    if (i > 15 && attemptFast) {
      WiFi.disconnect(true);
      vTaskDelay(50 / portTICK_PERIOD_MS);
      WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0),
                  IPAddress(0, 0, 0, 0));
      WiFi.begin(sStr.c_str(), pStr.c_str());
      attemptFast = false;
    }

    if (i > 60) {
      if (status == WL_NO_SSID_AVAIL)
        return 2;
      if (status == WL_CONNECT_FAILED)
        return 3;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  WiFi.disconnect();
  return 4; // Timeout
}

bool ensureWiFi(bool force = false) {
  if (!force && WiFi.status() == WL_CONNECTED)
    return true;

  currentState = SS_CONNECTING;
  uiLine1 = "WIFI...";
  uiLine2 = "";
  uiLine3 = "";

  preferences.begin("aether", true);
  int primarySlot = preferences.getInt("primSlot", 1);
  preferences.end();

  preferences.begin("aether_wifi", true);
  String s = preferences.getString(("s" + String(primarySlot)).c_str(), "");
  String p = preferences.getString(("p" + String(primarySlot)).c_str(), "");
  preferences.end();

  int res = 4;
  if (s.length() == 0) {
    for (int i = 1; i <= 5; i++) {
      if (buttonEvent) {
        buttonEvent = false;
        break;
      }
      preferences.begin("aether_wifi", true);
      s = preferences.getString(("s" + String(i)).c_str(), "");
      p = preferences.getString(("p" + String(i)).c_str(), "");
      preferences.end();
      if (s.length() > 0) {
        res = tryConnect(s.c_str(), p.c_str());
        if (res == 0) {
          if (validateIPReady())
            return true;
        }
        if (res == 1)
          break;
      }
    }
  } else {
    res = tryConnect(s.c_str(), p.c_str());
    if (res == 0) {
      if (validateIPReady())
        return true;
    }
  }

  // Handle Errors Verbously
  currentState = SS_MENU;
  if (res != 0 && res != 1) {
    const char *err = "TIMEOUT";
    if (res == 2) err = "NO SSID";
    if (res == 3) err = "AUTH";
    drawErrorScreen("WIFI", err, "TAP TO RETRY");
    vTaskDelay(1600 / portTICK_PERIOD_MS);
  }
  return false;
}

void runWiFiPortal() {
  Serial.println("[PORTAL] Starting WiFi Configuration Portal...");
  currentState = SS_PORTAL;
  WiFi.disconnect();
  vTaskDelay(500 / portTICK_PERIOD_MS);

  WiFi.mode(WIFI_AP_STA);
  // Secured AP with password
  if (WiFi.softAP("AETHER_CONFIG", "aether123")) {
    Serial.print(
        "[PORTAL] AP Started. SSID: AETHER_CONFIG, Pass: aether123, IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[PORTAL] AP Start Failed!");
  }

  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println("[PORTAL] DNS Server Started.");

  String html =
      "<html><head><meta charset='UTF-8'><meta name='viewport' "
      "content='width=device-width,initial-scale=1'><title>AETHER | WiFi "
      "Setup</title><style>"
      "body{margin:0;padding:0;background:#0a0a0a;color:#fff;font-family:-"
      "apple-system,BlinkMacSystemFont,'Segoe "
      "UI',Roboto,Helvetica,Arial,sans-serif;display:flex;align-items:center;"
      "justify-content:center;min-height:100vh;color-scheme:dark}"
      ".card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);"
      "border:1px solid "
      "rgba(255,255,255,0.1);padding:40px;border-radius:24px;width:90%;max-"
      "width:340px;box-shadow:0 20px 50px rgba(0,0,0,0.5);text-align:center}"
      "h1{font-size:24px;font-weight:700;letter-spacing:4px;margin:0 0 "
      "8px;background:linear-gradient(90deg,#00bcd4,#00acc1);-webkit-"
      "background-clip:text;-webkit-text-fill-color:transparent}"
      "p{color:rgba(255,255,255,0.5);font-size:14px;margin-bottom:32px}"
      "select,input{width:100%;background:#1a1a1a;border:1px solid "
      "rgba(255,255,255,0.1);border-radius:12px;padding:14px;margin-bottom:"
      "16px;color:#fff;font-size:16px;box-sizing:border-box;appearance:none;"
      "transition:all 0.3s}"
      "select:focus,input:focus{outline:none;border-color:#00bcd4;background:"
      "rgba(255,255,255,0.1)}"
      "option{background:#1a1a1a;color:#fff}"
      "button{width:100%;background:#00bcd4;color:#000;border:none;border-"
      "radius:12px;padding:16px;font-size:16px;font-weight:700;cursor:pointer;"
      "transition:transform 0.2s,background 0.3s;margin-top:8px}"
      "button:active{transform:scale(0.98);background:#00acc1}"
      "footer{margin-top:32px;font-size:10px;color:rgba(255,255,255,0.3);"
      "letter-spacing:1px}"
      "</style></head><body><div class='card'><h1>AETHER</h1><p>WIFI "
      "PROVISIONING</p>"
      "<form action='/save' method='POST'>"
      "<select name='slot'>"
      "<option value='1'>Profile Slot 1</option><option value='2'>Profile Slot "
      "2</option><option value='3'>Profile Slot 3</option>"
      "<option value='4'>Profile Slot 4</option><option value='5'>Profile Slot "
      "5</option></select>"
      "<input name='s' placeholder='Network SSID' required><input name='p' "
      "type='password' placeholder='Password'>"
      "<button type='submit'>STORE WIFI</button></form><footer>v2.3.1 // "
      "BANK_5_SLOTS_ACTIVE</footer></div></body></html>";

  server.on("/", HTTP_GET, [&html]() {
    Serial.println("[PORTAL] Client requested root page.");
    server.send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, []() {
    String s = server.arg("s");
    s.trim();
    String p = server.arg("p");
    p.trim();
    String slot = server.arg("slot");
    if (slot == "")
      slot = "1";

    Serial.printf("[PORTAL] Saving to Slot %s: %s\n", slot.c_str(), s.c_str());
    if (s.length() > 0) {
      Preferences pStore;
      pStore.begin("aether_wifi", false);
      pStore.putString(("s" + slot).c_str(), s);
      pStore.putString(("p" + slot).c_str(), p);
      pStore.end();
      server.send(200, "text/html",
                  "<h1>SAVED</h1><p>Slot " + slot +
                      " updated. Rebooting...</p>");
      delay(1000);
      ESP.restart();
    }
    server.send(400, "text/plain", "SSID REQUIRED");
  });

  // Captive Portal Redirects (Standard for Apple, Android, Windows)
  server.on("/generate_204", HTTP_GET,
            [&html]() { server.send(200, "text/html", html); });
  server.on("/fwlink", HTTP_GET,
            [&html]() { server.send(200, "text/html", html); });
  server.on("/hotspot-detect.html", HTTP_GET,
            [&html]() { server.send(200, "text/html", html); });
  server.on("/canonical.html", HTTP_GET,
            [&html]() { server.send(200, "text/html", html); });
  server.on("/success.txt", HTTP_GET,
            []() { server.send(200, "text/plain", "success"); });

  server.onNotFound([&html]() {
    String host = server.hostHeader();
    if (host != "192.168.4.1") {
      server.sendHeader("Location", "http://192.168.4.1/");
      server.send(302, "text/plain", "");
    } else {
      server.send(200, "text/html", html);
    }
  });

  server.begin();
  Serial.println("[PORTAL] Web Server Started.");

  buttonEvent = false;
  unsigned long portalStart = millis();
  while (millis() - portalStart < 180000) {
    dnsServer.processNextRequest();
    server.handleClient();
    if (buttonEvent) {
      Serial.println("[PORTAL] User cancelled portal via button.");
      buttonEvent = false;
      break;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  Serial.println("[PORTAL] Closing AP and resetting WiFi radio...");
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  WiFi.mode(WIFI_STA);

  currentState = SS_MENU;
  Serial.println("[PORTAL] Portal Closed.");
}

void showSavedWiFi() {
  int currentSlot = 1;
  int viewMode = 0; // 0: SSID List, 1: Password View
  lastInteractionTime = millis();

  // Load current Primary for indicator
  preferences.begin("aether", true);
  int primSlot = preferences.getInt("primSlot", 1);
  preferences.end();

  while (millis() - lastInteractionTime < 20000) {
    preferences.begin("aether_wifi", true);
    String sKey = "s" + String(currentSlot);
    String pKey = "p" + String(currentSlot);
    String s = "EMPTY";
    String p = "";

    if (preferences.isKey(sKey.c_str())) {
      s = preferences.getString(sKey.c_str(), "EMPTY");
      p = preferences.getString(pKey.c_str(), "");
    }
    preferences.end();

    if (dm::beginFrame(100)) {
      // Slot viewer redesign: big slot number, primary indicator, SSID prominent.
      char headBuf[16];
      snprintf(headBuf, sizeof(headBuf), "SLOT %d%s",
               currentSlot, (currentSlot == primSlot) ? " *" : "");
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, headBuf, false, dm::ICON_WIFI);

      if (viewMode == 0) {
        // SSID view: big text
        dm::setFont(dm::FONT_SMALL);
        dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 13, "SSID");
        dm::setFont(dm::FONT_LARGE);
        dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 20, s.substring(0, 6).c_str());
        // Slot progress dots at the right edge showing position 1..5
        for (int slot = 1; slot <= 5; slot++) {
          int dx = OLED_OFFSET_X + OLED_W - 8 + (slot - 1) * 0;  // stacked
          int dy = OLED_OFFSET_Y + 13 + (slot - 1) * 4;
          if (slot == currentSlot) dm::drawFilledCircle(dx, dy, 1);
          else                     dm::drawPixel(dx, dy);
        }
        dm::setFont(dm::FONT_SMALL);
        dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "[HOLD] PRIMARY");
      } else {
        // Password view: shown as bullet dots (privacy)
        dm::setFont(dm::FONT_SMALL);
        dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 13, "PASSWORD");
        int dotCount = p.length();
        if (dotCount > 10) dotCount = 10;
        int dx = OLED_OFFSET_X + 2;
        int dy = OLED_OFFSET_Y + 26;
        for (int i = 0; i < dotCount; i++) {
          dm::drawFilledCircle(dx + i * 5, dy, 1);
        }
        dm::setFont(dm::FONT_SMALL);
        dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "[TAP] BACK");
      }
      dm::endFrame();
    }

    if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
      if (!longPressTriggered) {
        longPressTriggered = true;
        // SET AS PRIMARY
        preferences.begin("aether", false);
        preferences.putInt("primSlot", currentSlot);
        preferences.end();
        primSlot = currentSlot; // Update indicator
        // Explicit confirmation screen (dm::toast can't overlay here).
        char confBuf[16];
        snprintf(confBuf, sizeof(confBuf), "SLOT %d", currentSlot);
        drawConfirmScreen("WIFI", confBuf, "SET AS PRIMARY");
        vTaskDelay(1400 / portTICK_PERIOD_MS);
        break;
      }
    }

    if (buttonEvent) {
      buttonEvent = false;
      if (viewMode == 1) {
        viewMode = 0;
      } else {
        currentSlot++;
        if (currentSlot > 5)
          currentSlot = 1;
      }
      lastInteractionTime = millis();
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void deleteSavedWiFi() {
  bool confirmed = drawConfirmPromptAndWait(
      "CLEAR WIFI", "ALL 5 SLOTS?",
      "HOLD=OK  TAP=NO", 6000);
  if (!confirmed) {
    drawConfirmScreen("WIFI", "KEPT", "CANCELLED");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return;
  }
  preferences.begin("aether_wifi", false);
  preferences.clear();
  preferences.end();
  drawConfirmScreen("WIFI", "CLEARED", "REBOOTING...");
  vTaskDelay(1500 / portTICK_PERIOD_MS);
  ESP.restart();
}

void runLocatePage() {
  if (!ensureWiFi()) {
    currentState = SS_MENU;
    return;
  }
  currentState = SS_LOCATING;
  uiLine1 = "FETCHING IP...";

  WiFiClient client;
  HTTPClient http;
  String url = "http://ip-api.com/json/?fields=status,city,lat,lon,offset";
  if (http.begin(client, url)) {
    int code = http.GET();
    // CRITICAL: reset state here so uiTask's SS_LOCATING pin-drop animation
    // stops before any confirmation/error screen is drawn.
    currentState = SS_MENU;
    if (code == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      if (doc["status"] == "success") {
        locLat = doc["lat"].as<float>();
        locLon = doc["lon"].as<float>();
        locOffset = doc["offset"].as<long>();
        String city = doc["city"].as<String>();
        strncpy(locCity, city.c_str(), 15);
        locCity[15] = '\0';
        locationSynced = true;

        // Save to Permanent NVS
        preferences.begin("aether", false);
        preferences.putBool("locSync", true);
        preferences.putFloat("lat", locLat);
        preferences.putFloat("lon", locLon);
        preferences.putLong("offset", locOffset);
        preferences.putString("city", String(locCity));
        preferences.end();

        String cleanCity = String(locCity);
        cleanCity.toUpperCase();
        drawLocateResult(cleanCity.c_str());
        waitWithButtonPoll(3000);
      } else {
        drawErrorScreen("LOCATE", "API", "IP-API FAILED");
        waitWithButtonPoll(2500);
      }
    } else {
      char buf[12]; snprintf(buf, sizeof(buf), "HTTP %d", code);
      drawErrorScreen("LOCATE", buf, "CHECK NET");
      waitWithButtonPoll(2500);
    }
    http.end();
  }
  currentState = SS_MENU;
}

void showTimePage() {
  if (!locationSynced) {
    drawLockedScreen(dm::ICON_TIME_LG, "CLOCK");
    waitWithButtonPoll(3000);
    return;
  }

  if (!ensureWiFi()) {
    currentState = SS_MENU;
    return;
  }
  currentState = SS_CONNECTING;
  uiLine1 = "NTP SYNC...";

  long tzHours = locOffset / 3600;
  String tzString =
      "UTC" + String(tzHours > 0 ? "-" : "+") + String(abs(tzHours));
  configTime(locOffset, 0, "pool.ntp.org");
  setenv("TZ", tzString.c_str(), 1);
  tzset();

  struct tm tinfo;
  int retries = 0;
  while (!getLocalTime(&tinfo) && retries < 10) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    retries++;
  }
  currentState = SS_MENU;

  unsigned long start = millis();
  String cleanCity = String(locCity);
  cleanCity.toUpperCase();

  while (millis() - start < 8000) {
    if (getLocalTime(&tinfo)) {
      char hhmm[6], ss[4], dStr[12], dayStr[16];
      strftime(hhmm, sizeof(hhmm), "%H:%M", &tinfo);
      strftime(ss,   sizeof(ss),   "%S",    &tinfo);
      strftime(dStr, sizeof(dStr), "%d/%m", &tinfo);
      strftime(dayStr, sizeof(dayStr), "%a", &tinfo);
      String day = String(dayStr);
      day.toUpperCase();
      drawClockScreen(hhmm, ss, dStr, day.c_str());
    }
    if (waitWithButtonPoll(500))
      break; // Cancel on button press
  }
}

void showWeatherPage() {
  if (!locationSynced) {
    drawLockedScreen(dm::ICON_WEATHER_LG, "WEATHER");
    waitWithButtonPoll(3000);
    return;
  }

  if (!ensureWiFi()) {
    currentState = SS_MENU;
    return;
  }
  currentState = SS_SYNCING;
  uiLine1 = "FETCHING";
  uiLine2 = "DATA...";
  uiLine3 = "";

#ifdef WEATHER_API_KEY
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" +
               String(locLat, 4) + "&lon=" + String(locLon, 4) +
               "&units=metric&appid=" + String(WEATHER_API_KEY);
  if (http.begin(client, url)) {
    int code = http.GET();
    // Reset state before drawing result — otherwise SS_SYNCING cloud+orbit
    // animation from uiTask overlays the weather screen.
    currentState = SS_MENU;
    if (code == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      float temp = doc["main"]["temp"].as<float>();
      int hum = doc["main"]["humidity"].as<int>();
      String desc = doc["weather"][0]["main"].as<String>();
      desc.toUpperCase();
      drawWeatherScreen(temp, hum, desc.c_str());
    } else {
      char buf[12]; snprintf(buf, sizeof(buf), "HTTP %d", code);
      drawErrorScreen("WEATHER", buf, "API FAILED");
    }
    http.end();
  } else {
    drawErrorScreen("WEATHER", "BEGIN", "HTTP INIT");
  }
#else
  drawWeatherScreen(30.5f, 68, "SUNNY");
#endif
  // IMPORTANT: reset state BEFORE the display wait, otherwise uiTask keeps
  // rendering the SS_SYNCING spinner over drawMenu() and the button handler
  // (which only advances menu when state == SS_MENU/SS_WIFI_MENU) ignores presses.
  currentState = SS_MENU;
  waitWithButtonPoll(8000);
}

void runMeasurementFlow(String trigger) {
  if (trigger == "manual") {
    // Splash: big scan icon + AETHER label
    if (dm::beginFrame(portMAX_DELAY)) {
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "AETHER", true, dm::ICON_SCAN);
      dm::drawIcon24(OLED_OFFSET_X + (OLED_W - 24) / 2, OLED_OFFSET_Y + 12, dm::ICON_MEASURE_LG);
      dm::setFont(dm::FONT_NORMAL);
      const char *msg = "SCAN 5x";
      int w = dm::textWidth(msg);
      dm::drawText(OLED_OFFSET_X + (OLED_W - w) / 2, OLED_OFFSET_Y + OLED_H - 10, msg);
      dm::endFrame();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  float totalT = 0, totalH = 0, totalA = 0;
  int totalL = 0, validCount = 0;
  String sampleLog = "Scans: ";

  for (int i = 0; i < 5; i++) {
    // Check for long press to exit instantly
    unsigned long stepStart = millis();
    while (millis() - stepStart < 2500) {
      if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
        dm::toast("EXIT SCAN", 800);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        return;
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    setLED(i * 40, 200, 0);
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int l = analogRead(LDR_PIN);
    float acc = 0;
    if (mpuFound) {
      sensors_event_t ae, ge, te;
      // NOTE: displayMutex now guards the shared Wire bus (SSD1306 + MPU6050).
      // Without this, an OLED refresh mid-getEvent() can corrupt the I2C txn.
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool ok = mpu.getEvent(&ae, &ge, &te);
        xSemaphoreGive(displayMutex);
        if (ok)
          acc = sqrt(sq(ae.acceleration.x) + sq(ae.acceleration.y) +
                     sq(ae.acceleration.z));
      }
    }

    String headerStr = "SCAN " + String(i + 1) + "/5";
    if (!isnan(t) && !isnan(h) && h <= 100.0 && t < 60.0) {
      totalT += t;
      totalH += h;
      totalL += l;
      totalA += acc;
      validCount++;
      sampleLog += "[T:" + String(t, 1) + " H:" + String(h, 0) + "] ";
      drawMeasureSample(i + 1, 5, t, (int)h, l);
    } else {
      sampleLog += "[ERR] ";
      drawMeasureGlitch(i + 1, 5);
    }
  }

  if (validCount > 0) {
    // Append to the local trend history immediately — this must happen
    // before any WiFi/upload attempt so a connectivity failure never causes
    // the on-device history to silently fall behind reality.
    appendTrendPoint(totalT / validCount, totalH / validCount,
                     (int)(totalL / validCount));

    uiLine1 = "LINKING...";
    if (!ensureWiFi(true)) {
      currentState = SS_MENU;
      drawErrorScreen("SYNC", "WIFI", "LINK DOWN");
      vTaskDelay(1500 / portTICK_PERIOD_MS);
      return;
    }

    currentState = SS_SYNCING;
    uiLine1 = "SYNCING...";
    Serial.printf("[SYSTEM] Free Heap: %d bytes\n", ESP.getFreeHeap());

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setReuse(true);

    // Task 1: Debug Log
    if (http.begin(client, String(SUPABASE_URL) + "/rest/v1/device_logs")) {
      http.addHeader("apikey", SUPABASE_KEY);
      http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      http.addHeader("Content-Type", "application/json");
      JsonDocument logDoc;
      logDoc["message"] = sampleLog;
      logDoc["level"] = "DEBUG";
      String logBody;
      serializeJson(logDoc, logBody);
      http.POST(logBody);
    }

    // Task 2: Data Upload
    bool uploadOk = false;
    if (http.begin(client, String(SUPABASE_URL) + "/rest/v1/room_readings")) {
      http.addHeader("apikey", SUPABASE_KEY);
      http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      http.addHeader("Content-Type", "application/json");
      JsonDocument doc;
      doc["temperature"] = totalT / validCount;
      doc["humidity"] = totalH / validCount;
      doc["ldr_value"] = totalL / validCount;
      doc["accel_total"] = totalA / validCount;
      doc["trigger_source"] = trigger;
      doc["battery_v"] = 3.3;
      String body;
      serializeJson(doc, body);
      int code = http.POST(body);
      Serial.printf("[UPLOAD] room_readings HTTP %d\n", code);
      if (code >= 200 && code < 300) {
        uploadOk = true;
        measureCount++;
        saveWiFiSnapshot(); // Commit valid connection to NVS
        preferences.begin("stats", false);
        preferences.putInt("measures", measureCount);
        preferences.end();

        // Sync Runtime Log Queue
        preferences.begin("logs_v2", true);
        preferences.getBytes("queue", &logQueue, sizeof(LogQueue));
        preferences.end();

        if (logQueue.count > 0) {
          if (http.begin(client,
                         String(SUPABASE_URL) + "/rest/v1/device_sessions")) {
            http.addHeader("apikey", SUPABASE_KEY);
            http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Prefer", "return=minimal");

            JsonDocument sessionDoc;
            for (int i = 0; i < logQueue.count; i++) {
              JsonObject obj = sessionDoc.add<JsonObject>();
              obj["start_time"] = logQueue.logs[i].startTime;
              obj["duration"] = logQueue.logs[i].duration;
              obj["boot_count"] = bootCount;
              obj["measure_count"] = measureCount;
              obj["sleep_interval"] = config.sleepMinutes;
            }
            String sessionBody;
            serializeJson(sessionDoc, sessionBody);
            int sessCode = http.POST(sessionBody);
            if (sessCode >= 200 && sessCode < 300) {
              logQueue.count = 0;
              preferences.begin("logs_v2", false);
              preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
              preferences.end();
            }
          }
        }
      }
      http.end();
    }
    // ALWAYS reset state so uiTask/button handling can resume even on failure.
    currentState = SS_MENU;
    if (uploadOk) {
      // Explicit success screen (a toast can't overlay here because
      // g_menuOwnedByPage suppresses uiTask rendering during this blocking wait).
      drawConfirmScreen("CLOUD", "SYNCED", "DATA SAVED");
      vTaskDelay(1500 / portTICK_PERIOD_MS);
    } else {
      drawErrorScreen("CLOUD", "UPLOAD", "RETRY LATER");
      vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
  } else {
    drawErrorScreen("SENSOR", "NO DATA", "CHECK WIRES");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void runResetStats() {
  bool ok = drawConfirmPromptAndWait("RESET STATS", "CLEAR ALL?",
                                     "HOLD=OK  TAP=NO", 5000);
  if (!ok) {
    drawConfirmScreen("STATS", "KEPT", "CANCELLED");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return;
  }
  bootCount = 0;
  measureCount = 0;
  preferences.begin("stats", false);
  preferences.putInt("boots", 0);
  preferences.putInt("measures", 0);
  preferences.end();
  drawConfirmScreen("STATS", "CLEARED", "0 BOOTS 0 SCANS");
  vTaskDelay(1400 / portTICK_PERIOD_MS);
}

// --- Custom page renderers -------------------------------------------------
// Each of these takes the display mutex via dm::beginFrame/endFrame, draws a
// bespoke layout, and returns. Called from monitorTask while g_menuOwnedByPage
// is true so uiTask doesn't overwrite them.

static void drawClockScreen(const char *hhmm, const char *ss,
                            const char *ddmmyy, const char *day) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, day, false, dm::ICON_WIFI);

  // Big HH:MM centred vertically in the body area.
  dm::setFont(dm::FONT_HUGE);
  int w = dm::textWidth(hhmm);  // width measured WITH the colon so centering
                                 // never shifts when the colon blinks off.
  int x = OLED_OFFSET_X + (OLED_W - w) / 2;
  if (x < 0) x = 0;

  int secInt = (ss && ss[0] && ss[1]) ? ((ss[0]-'0')*10 + (ss[1]-'0')) : 0;

  // Classic digital-clock heartbeat: blink the ':' itself (swap to a space on
  // odd seconds) rather than an unrelated side indicator. Same string length
  // is preserved so the centred x position never jumps.
  char blinked[8];
  strncpy(blinked, hhmm, sizeof(blinked) - 1);
  blinked[sizeof(blinked) - 1] = '\0';
  if (secInt & 1) {
    for (char *p = blinked; *p; p++) {
      if (*p == ':') *p = ' ';
    }
  }
  dm::drawText(x, OLED_OFFSET_Y + 12, blinked);  // occupies y=12..30

  // Footer inverted bar: date on left, ticking seconds on right.
  // Bar top at y=39 keeps a 8-px gap under the digits (which end at y=30).
  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  dm::drawTextInverted(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, ddmmyy);
  int sw = dm::textWidth(ss);
  dm::drawTextInverted(OLED_OFFSET_X + OLED_W - sw - 2, OLED_OFFSET_Y + OLED_H - 8, ss);

  // Seconds progress bar just above the footer (y=35..36), safely below the
  // digits' descenders (which end at y=31).
  int barX = OLED_OFFSET_X + 4;
  int barY = OLED_OFFSET_Y + 35;
  int barMaxW = OLED_W - 8;
  int barFill = (barMaxW * secInt) / 60;
  dm::drawHLine(barX, barY + 1, barMaxW);
  if (barFill > 0) dm::drawFilledRect(barX, barY, barFill, 2);
  dm::endFrame();
}

// Two-column value renderer used by Weather/Stats/Measure.
// Draws a centred SMALL label on the top row and a centred HUGE numeric value
// on the middle row. Guarantees the value never overflows the column (long
// values are shrunk to FONT_LARGE, then FONT_NORMAL, then truncated).
static void drawColumnValue(int colX, int colW, int topY,
                            const char *label, const char *value) {
  // Label row (7 px tall)
  dm::setFont(dm::FONT_SMALL);
  int lw = dm::textWidth(label);
  if (lw > colW) lw = colW;
  int lx = colX + (colW - lw) / 2;
  dm::drawText(lx, topY, label);

  // Value row — try HUGE first, downgrade if it doesn't fit.
  const uint16_t VALUE_Y_OFFSET = 8;   // just below the label
  dm::Font trials[] = { dm::FONT_HUGE, dm::FONT_LARGE, dm::FONT_NORMAL };
  int vw = 0;
  int pickedFont = 2;
  for (int i = 0; i < 3; i++) {
    dm::setFont(trials[i]);
    vw = dm::textWidth(value);
    if (vw <= colW - 2) { pickedFont = i; break; }
  }
  dm::setFont(trials[pickedFont]);
  int vx = colX + (colW - vw) / 2;
  if (vx < colX) vx = colX;
  dm::drawText(vx, topY + VALUE_Y_OFFSET, value);
}
static void drawWeatherScreen(float tempC, int humPct, const char *desc) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "WEATHER", false, dm::ICON_WIFI);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%d", tInt);
  snprintf(humBuf,  sizeof(humBuf),  "%d", humPct);

  // Two columns, unit lives in the label row so nothing collides horizontally.
  drawColumnValue(OLED_OFFSET_X + 2, 28, OLED_OFFSET_Y + 11, "TEMP C", tempBuf);
  dm::drawVLine(OLED_OFFSET_X + 32, OLED_OFFSET_Y + 11, 26);
  drawColumnValue(OLED_OFFSET_X + 34, 28, OLED_OFFSET_Y + 11, "HUM %", humBuf);

  // Footer inverted bar with condition text.
  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  int dw = dm::textWidth(desc);
  int dx = OLED_OFFSET_X + (OLED_W - dw) / 2;
  if (dx < 2) dx = 2;
  dm::drawTextInverted(dx, OLED_OFFSET_Y + OLED_H - 8, desc);
  dm::endFrame();
}

static void drawStatsScreen(uint32_t measures, uint32_t boots, uint32_t uptimeMin) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "STATS", false, dm::ICON_WIFI);

  auto fmtCompact = [](uint32_t v, char *out, size_t n) {
    if (v < 1000)         snprintf(out, n, "%lu",  (unsigned long)v);
    else if (v < 10000)   snprintf(out, n, "%lu.%luk",
                                   (unsigned long)(v/1000),
                                   (unsigned long)((v%1000)/100));
    else                  snprintf(out, n, "%luk", (unsigned long)(v/1000));
  };
  char mb[10], bb[10];
  fmtCompact(measures, mb, sizeof(mb));
  fmtCompact(boots,    bb, sizeof(bb));

  drawColumnValue(OLED_OFFSET_X + 2,  28, OLED_OFFSET_Y + 11, "MEAS", mb);
  dm::drawVLine(OLED_OFFSET_X + 32, OLED_OFFSET_Y + 11, 26);
  drawColumnValue(OLED_OFFSET_X + 34, 28, OLED_OFFSET_Y + 11, "BOOT", bb);

  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  char up[20];
  if (uptimeMin < 60)         snprintf(up, sizeof(up), "UP %lum",  (unsigned long)uptimeMin);
  else if (uptimeMin < 1440)  snprintf(up, sizeof(up), "UP %luh %lum",
                                       (unsigned long)(uptimeMin/60),
                                       (unsigned long)(uptimeMin%60));
  else                        snprintf(up, sizeof(up), "UP %lud %luh",
                                       (unsigned long)(uptimeMin/1440),
                                       (unsigned long)((uptimeMin%1440)/60));
  dm::setFont(dm::FONT_SMALL);
  int uw = dm::textWidth(up);
  int ux = OLED_OFFSET_X + (OLED_W - uw) / 2;
  if (ux < 2) ux = 2;
  dm::drawTextInverted(ux, OLED_OFFSET_Y + OLED_H - 8, up);
  dm::endFrame();
}

// LOCATE result: pin icon + big city name.
static void drawLocateResult(const char *city) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "LOCATE", true, dm::ICON_PIN);
  dm::drawIcon24(OLED_OFFSET_X + (OLED_W - 24) / 2, OLED_OFFSET_Y + 12, dm::ICON_LOCATE_LG);
  dm::setFont(dm::FONT_NORMAL);
  int w = dm::textWidth(city);
  int x = OLED_OFFSET_X + (OLED_W - w) / 2;
  if (x < 0) x = 0;
  dm::drawText(x, OLED_OFFSET_Y + OLED_H - 10, city);
  dm::endFrame();
}

static void drawMeasureSample(int sampleIdx, int totalSamples,
                              float tempC, int humPct, int ldr) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  char headBuf[16];
  snprintf(headBuf, sizeof(headBuf), "SCAN %d/%d", sampleIdx, totalSamples);
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, headBuf, true, dm::ICON_SCAN);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  if (humPct > 99) humPct = 99;
  if (humPct < 0)  humPct = 0;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%d", tInt);
  snprintf(humBuf,  sizeof(humBuf),  "%d", humPct);

  drawColumnValue(OLED_OFFSET_X + 2,  28, OLED_OFFSET_Y + 11, "TEMP C", tempBuf);
  dm::drawVLine(OLED_OFFSET_X + 32, OLED_OFFSET_Y + 11, 26);
  drawColumnValue(OLED_OFFSET_X + 34, 28, OLED_OFFSET_Y + 11, "HUM %", humBuf);

  // Footer: LDR value in inverted bar.
  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  char lb[12]; snprintf(lb, sizeof(lb), "LDR %d", ldr);
  int lw = dm::textWidth(lb);
  int lx = OLED_OFFSET_X + (OLED_W - lw) / 2;
  if (lx < 2) lx = 2;
  dm::drawTextInverted(lx, OLED_OFFSET_Y + OLED_H - 8, lb);
  dm::endFrame();
}

// Sensor GLITCH screen — bold X marker.
static void drawMeasureGlitch(int sampleIdx, int totalSamples) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  char headBuf[16];
  snprintf(headBuf, sizeof(headBuf), "SCAN %d/%d", sampleIdx, totalSamples);
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, headBuf, true, dm::ICON_SCAN);
  // X mark centre
  int cx = OLED_OFFSET_X + OLED_W / 2;
  int cy = OLED_OFFSET_Y + 26;
  dm::drawLine(cx - 8, cy - 8, cx + 8, cy + 8);
  dm::drawLine(cx - 8, cy + 8, cx + 8, cy - 8);
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(OLED_OFFSET_X + 8, OLED_OFFSET_Y + OLED_H - 8, "GLITCH");
  dm::endFrame();
}

// Bespoke error screen: header title inverted (red-ish feel on monochrome),
// bold X + large error code, small hint at bottom.
static void drawErrorScreen(const char *title, const char *code, const char *hint) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, title, false, dm::ICON_WIFI);

  // Left half: big X mark
  int cx = OLED_OFFSET_X + 12;
  int cy = OLED_OFFSET_Y + 26;
  int r  = 9;
  dm::drawCircle(cx, cy, r);
  dm::drawLine(cx - 5, cy - 5, cx + 5, cy + 5);
  dm::drawLine(cx - 5, cy + 5, cx + 5, cy - 5);

  // Right half: code (large) and hint (small)
  if (code && code[0]) {
    dm::setFont(dm::FONT_NORMAL);
    dm::drawText(OLED_OFFSET_X + 26, OLED_OFFSET_Y + 18, code);
  }
  if (hint && hint[0]) {
    dm::setFont(dm::FONT_SMALL);
    dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, hint);
  }
  dm::endFrame();
}

// Locked-state pre-req screen: dimmed feature icon + arrow → LOCATE prompt.
static void drawLockedScreen(dm::Icon featureIcon, const char *featureName) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, featureName, false, dm::ICON_WIFI);

  // Feature icon (dimmed by drawing then punching a checker mask)
  int ix = OLED_OFFSET_X + 4;
  int iy = OLED_OFFSET_Y + 12;
  dm::drawIcon24(ix, iy, featureIcon);
  // Dither mask over the icon: overlay every-other pixel as bg to simulate dim.
  for (int y = iy; y < iy + 24; y++) {
    for (int x = ix; x < ix + 24; x++) {
      if (((x + y) & 1) == 0) {
        dm::clearRect(x, y, 1, 1);
      }
    }
  }

  // Arrow "→" (drawn as line + head)
  int ax = OLED_OFFSET_X + 30, ay = OLED_OFFSET_Y + 24;
  dm::drawHLine(ax, ay, 8);
  dm::drawLine(ax + 8, ay, ax + 5, ay - 3);
  dm::drawLine(ax + 8, ay, ax + 5, ay + 3);

  // Prompt icon (small locate pin) + label
  dm::drawIcon24(OLED_OFFSET_X + OLED_W - 24, OLED_OFFSET_Y + 12, dm::ICON_LOCATE_LG);
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "RUN LOCATE FIRST");
  dm::endFrame();
}

// WiFi connecting splash used inside tryConnect(). Shows an expanding-arc
// pulse around a small WiFi glyph. `elapsedMs` drives the arc animation.
static void drawWifiConnecting(const char *ssid, int elapsedMs) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "LINKING", false, dm::ICON_WIFI);
  int cx = OLED_OFFSET_X + 14;
  int cy = OLED_OFFSET_Y + 26;
  // Central dot
  dm::drawFilledCircle(cx, cy, 2);
  // Three expanding arc rings, phase-shifted by elapsedMs (period 1200ms)
  int phase = (elapsedMs / 100) % 12;
  for (int i = 0; i < 3; i++) {
    int r = ((phase + i * 4) % 12) + 3;   // 3..14
    if (r >= 4 && r <= 12) dm::drawCircle(cx, cy, r);
  }
  // SSID (truncated)
  dm::setFont(dm::FONT_NORMAL);
  char sbuf[10];
  if (ssid) { strncpy(sbuf, ssid, 9); sbuf[9] = 0; }
  else      { sbuf[0] = 0; }
  dm::drawText(OLED_OFFSET_X + 28, OLED_OFFSET_Y + 20, sbuf);
  dm::setFont(dm::FONT_SMALL);
  dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, "[TAP] CANCEL");
  dm::endFrame();
}

// Bespoke success/confirmation screen. Uses a filled disc + inverted checkmark
// left, title in header, big line + small hint on the right.
static void drawConfirmScreen(const char *title, const char *big, const char *hint) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, title, false, dm::ICON_WIFI);
  // Filled disc with a punched checkmark
  int cx = OLED_OFFSET_X + 12;
  int cy = OLED_OFFSET_Y + 26;
  int r  = 9;
  dm::drawFilledCircle(cx, cy, r);
  // Checkmark cut out (bg colour lines)
  int ax = cx - 4, ay = cy;
  int bx = cx - 1, by = cy + 3;
  int c2x = cx + 4, c2y = cy - 3;
  // draw two thick lines by drawing 2 parallel lines
  for (int off = 0; off <= 1; off++) {
    dm::clearRect(ax,   ay + off, 1, 1);
    dm::clearRect(ax+1, ay+1+off, 1, 1);
    dm::clearRect(bx-1, by-1+off, 1, 1);
    dm::clearRect(bx,   by+off,   1, 1);
    dm::clearRect(bx+1, by-1+off, 1, 1);
    dm::clearRect(bx+2, by-2+off, 1, 1);
    dm::clearRect(bx+3, by-3+off, 1, 1);
    dm::clearRect(c2x-1, c2y+1+off, 1, 1);
    dm::clearRect(c2x,   c2y+off,   1, 1);
  }
  if (big && big[0]) {
    dm::setFont(dm::FONT_NORMAL);
    dm::drawText(OLED_OFFSET_X + 26, OLED_OFFSET_Y + 18, big);
  }
  if (hint && hint[0]) {
    dm::setFont(dm::FONT_SMALL);
    dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 8, hint);
  }
  dm::endFrame();
}

// Prompt screen: shows a title + body + two-line hints ("HOLD OK" / "TAP NO")
// and blocks until either the user long-presses (returns true), taps (returns
// false), or the timeout elapses (returns false).
static bool drawConfirmPromptAndWait(const char *title, const char *body,
                                     const char *hint, uint32_t timeoutMs) {
  (void)hint;   // hint arg kept for API stability but hints are now inlined
  uint32_t start = millis();
  bool longPressSeen = false;
  buttonEvent = false;
  while (millis() - start < timeoutMs) {
    if (dm::beginFrame(50)) {
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, title, false, dm::ICON_WIFI);
      dm::setFont(dm::FONT_NORMAL);
      dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 14, body);
      // Two-line hint stack so nothing overflows the 64 px width.
      dm::setFont(dm::FONT_SMALL);
      dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 28, "HOLD = OK");
      dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 36, "TAP  = NO");
      // Countdown ring
      uint32_t remainMs = timeoutMs - (millis() - start);
      int pct = (int)((remainMs * 100) / timeoutMs);
      dm::drawRect(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 3,
                   OLED_W - 4, 2);
      int fill = ((OLED_W - 6) * pct) / 100;
      if (fill > 0) dm::drawFilledRect(OLED_OFFSET_X + 3, OLED_OFFSET_Y + OLED_H - 2, fill, 0);
      dm::endFrame();
    }
    if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
      if (!longPressTriggered) {
        longPressTriggered = true;
        longPressSeen = true;
        return true;
      }
    }
    if (buttonEvent) { buttonEvent = false; return false; }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
  return longPressSeen;
}

void showStatsPage() {
  uint32_t totalMin = config.totalLifetimeRuntime / 60;
  drawStatsScreen(measureCount, bootCount, totalMin);
  waitWithButtonPoll(5000);
}

// ROOM: instant local DHT11+LDR snapshot, no WiFi/upload, no history append
// (kept separate from the quality-checked 5-sample MEASURE average).
static void drawRoomStatus(float tempC, int humPct, int ldrRaw,
                           const char *comfortTag, const char *lightTag) {
  if (!dm::beginFrame(portMAX_DELAY)) return;
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "ROOM", false, dm::ICON_WIFI);

  // Light-level tag in place of the WiFi icon slot (ROOM makes no network
  // calls, so the WiFi indicator would be misleading here).
  dm::setFont(dm::FONT_SMALL);
  int ltw = dm::textWidth(lightTag);
  dm::drawTextInverted(OLED_OFFSET_X + OLED_W - ltw - 2, OLED_OFFSET_Y + 2, lightTag);

  int tInt = (int)(tempC + 0.5f);
  if (tInt > 99) tInt = 99;
  if (tInt < -9) tInt = -9;
  char tempBuf[6], humBuf[6];
  snprintf(tempBuf, sizeof(tempBuf), "%d", tInt);
  snprintf(humBuf, sizeof(humBuf), "%d", humPct);

  drawColumnValue(OLED_OFFSET_X + 2, 28, OLED_OFFSET_Y + 11, "TEMP C", tempBuf);
  dm::drawVLine(OLED_OFFSET_X + 32, OLED_OFFSET_Y + 11, 26);
  drawColumnValue(OLED_OFFSET_X + 34, 28, OLED_OFFSET_Y + 11, "HUM %", humBuf);

  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  int cw = dm::textWidth(comfortTag);
  int cx = OLED_OFFSET_X + (OLED_W - cw) / 2;
  if (cx < 2) cx = 2;
  dm::drawTextInverted(cx, OLED_OFFSET_Y + OLED_H - 8, comfortTag);
  dm::endFrame();
}

void showRoomPage() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int ldrRaw = analogRead(LDR_PIN);

  if (isnan(t) || isnan(h)) {
    drawErrorScreen("ROOM", "SENSOR", "READ FAIL");
    waitWithButtonPoll(2000);
    return;
  }

  int tInt = (int)(t + 0.5f);
  const char *tempTag = (tInt < ROOM_TEMP_COLD_MAX)   ? "COLD"
                       : (tInt < ROOM_TEMP_COOL_MAX)   ? "COOL"
                       : (tInt < ROOM_TEMP_WARM_MAX)   ? "WARM"
                                                        : "HOT";
  int hInt = (int)(h + 0.5f);
  const char *humTag = (hInt < ROOM_HUM_DRY_MAX)      ? "DRY"
                      : (hInt < ROOM_HUM_NORMAL_MAX)   ? "NORMAL"
                                                        : "HUMID";
  char comfortTag[16];
  snprintf(comfortTag, sizeof(comfortTag), "%s+%s", tempTag, humTag);

  // NOTE: this LDR's voltage-divider wiring reads LOW when the room is
  // bright and HIGH when dark (verified on hardware) — the opposite of the
  // "higher = brighter" assumption a photoresistor-on-top divider would give.
  const char *lightTag = (ldrRaw < ROOM_LDR_BRIGHT_MAX)   ? "BRIGHT"
                        : (ldrRaw < ROOM_LDR_DARK_MIN)     ? "DIM"
                                                             : "DARK";

  drawRoomStatus(t, hInt, ldrRaw, comfortTag, lightTag);
  waitWithButtonPoll(5000);
}

enum TrendView { TREND_TEMP, TREND_HUM, TREND_LIGHT };

// Renders one sub-view of the TREND sparkline. `view` selects which metric
// from trendHistory to plot. Points are read out in chronological order
// (oldest first) using the ring-buffer math matching appendTrendPoint()'s
// "head = index of most recently written point" convention.
static void drawTrendSparkline(TrendView view) {
  if (!dm::beginFrame(portMAX_DELAY)) return;

  const char *name = (view == TREND_TEMP) ? "TEMP"
                    : (view == TREND_HUM) ? "HUM"
                                           : "LIGHT";

  if (trendHistory.count < 2) {
    dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, name, false, dm::ICON_WIFI);
    dm::setFont(dm::FONT_NORMAL);
    dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 18, "NOT ENOUGH");
    dm::setFont(dm::FONT_SMALL);
    dm::drawText(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 30, "DATA YET");
    dm::endFrame();
    return;
  }

  int n = trendHistory.count;
  float vals[12];
  for (int i = 0; i < n; i++) {
    int idx = (n < 12) ? i : (trendHistory.head + 1 + i) % 12;
    TrendPoint &p = trendHistory.points[idx];
    if (view == TREND_TEMP)      vals[i] = p.tempX10 / 10.0f;
    else if (view == TREND_HUM)  vals[i] = (float)p.humidity;
    else                          vals[i] = (float)p.ldr;
  }

  float delta = vals[n - 1] - vals[n - 2];
  const char *arrow = (delta > 0.3f) ? "^" : (delta < -0.3f) ? "v" : "-";
  char headBuf[16];
  snprintf(headBuf, sizeof(headBuf), "%s %s", name, arrow);
  dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, headBuf, false, dm::ICON_WIFI);

  float vMin = vals[0], vMax = vals[0];
  for (int i = 1; i < n; i++) {
    if (vals[i] < vMin) vMin = vals[i];
    if (vals[i] > vMax) vMax = vals[i];
  }
  float range = vMax - vMin;
  if (range < 0.01f) range = 1.0f; // flat-line guard, avoids div-by-zero

  int plotX0 = OLED_OFFSET_X + 2;
  int plotX1 = OLED_OFFSET_X + OLED_W - 2;
  int plotY0 = OLED_OFFSET_Y + 12;
  int plotY1 = OLED_OFFSET_Y + 36;
  int plotH = plotY1 - plotY0;
  int plotW = plotX1 - plotX0;

  int prevX = plotX0;
  int prevY = plotY1 - (int)(((vals[0] - vMin) / range) * plotH);
  for (int i = 1; i < n; i++) {
    int x = plotX0 + (plotW * i) / (n - 1);
    int y = plotY1 - (int)(((vals[i] - vMin) / range) * plotH);
    dm::drawLine(prevX, prevY, x, y);
    prevX = x;
    prevY = y;
  }

  char footBuf[16];
  if (view == TREND_TEMP)      snprintf(footBuf, sizeof(footBuf), "%.1fC", vals[n - 1]);
  else if (view == TREND_HUM)  snprintf(footBuf, sizeof(footBuf), "%d%%", (int)vals[n - 1]);
  else                          snprintf(footBuf, sizeof(footBuf), "%d", (int)vals[n - 1]);

  dm::drawFilledRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - 9, OLED_W, 9);
  dm::setFont(dm::FONT_SMALL);
  int fw = dm::textWidth(footBuf);
  int fx = OLED_OFFSET_X + (OLED_W - fw) / 2;
  if (fx < 2) fx = 2;
  dm::drawTextInverted(fx, OLED_OFFSET_Y + OLED_H - 8, footBuf);
  dm::endFrame();
}

void showTrendPage() {
  int view = 0; // 0=TEMP, 1=HUM, 2=LIGHT
  lastInteractionTime = millis();
  buttonEvent = false;

  while (millis() - lastInteractionTime < 20000) {
    drawTrendSparkline((TrendView)view);

    if (buttonEvent) {
      buttonEvent = false;
      view = (view + 1) % 3;
      lastInteractionTime = millis();
    }
    if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
      if (!longPressTriggered) {
        longPressTriggered = true;
        break;
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// drawMenu() is now a no-op wrapper (uiTask owns menu rendering). Kept for
// callers that still invoke it during startup; harmless when nothing draws.
void drawMenu() { /* moved to uiTask */ }

void enterDeepSleep() {
  currentState = SS_SLEEPING;
  g_menuOwnedByPage = true;   // Suppress uiTask menu render during sleep anim
  if (oledFound) {
    // Sleep sequence, three visually-distinct phases:
    //   Phase 1 (0..600ms):  "GOOD NITE" reveal — text wipes in from top
    //   Phase 2 (600..1000ms): CRT-off vertical collapse — top+bottom black
    //                          bars converge to a bright midline
    //   Phase 3 (1000..1200ms): midline shrinks horizontally to a centre dot,
    //                          then a brief flash, then black.
    const int P1_FRAMES = 12;
    const int P2_FRAMES = 8;
    const int P3_FRAMES = 4;

    // Phase 1: reveal via retracting bg mask from bottom.
    for (int i = 0; i < P1_FRAMES; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "SLEEP", false, dm::ICON_WIFI);
      dm::setFont(dm::FONT_HUGE);
      const char *msg = "GOOD";
      int mw = dm::textWidth(msg);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw) / 2, OLED_OFFSET_Y + 12, msg);
      dm::setFont(dm::FONT_LARGE);
      const char *msg2 = "NITE";
      int mw2 = dm::textWidth(msg2);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw2) / 2, OLED_OFFSET_Y + 30, msg2);
      // Retract bg mask from bottom
      int revealH = (OLED_H - 10) * (P1_FRAMES - i) / P1_FRAMES;
      if (revealH > 0) {
        dm::clearRect(OLED_OFFSET_X, OLED_OFFSET_Y + OLED_H - revealH,
                      OLED_W, revealH);
      }
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // Phase 2: CRT-off vertical collapse.
    for (int i = 0; i <= P2_FRAMES; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      dm::drawHeader(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, "SLEEP", false, dm::ICON_WIFI);
      dm::setFont(dm::FONT_HUGE);
      const char *msg = "GOOD";
      int mw = dm::textWidth(msg);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw) / 2, OLED_OFFSET_Y + 12, msg);
      dm::setFont(dm::FONT_LARGE);
      const char *msg2 = "NITE";
      int mw2 = dm::textWidth(msg2);
      dm::drawText(OLED_OFFSET_X + (OLED_W - mw2) / 2, OLED_OFFSET_Y + 30, msg2);

      int midY = OLED_OFFSET_Y + 10 + (OLED_H - 10) / 2;
      int barMax = (OLED_H - 10) / 2;
      int barH = (barMax * i) / P2_FRAMES;
      dm::clearRect(OLED_OFFSET_X, midY - barH, OLED_W, barH);
      dm::clearRect(OLED_OFFSET_X, midY, OLED_W, barH);
      dm::drawHLine(OLED_OFFSET_X, midY, OLED_W);
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    // Phase 3: horizontal collapse of the midline to a centre dot.
    for (int i = 0; i <= P3_FRAMES; i++) {
      if (!dm::beginFrame(portMAX_DELAY)) break;
      int midY = OLED_OFFSET_Y + 10 + (OLED_H - 10) / 2;
      int lineW = (OLED_W * (P3_FRAMES - i)) / P3_FRAMES;
      int lineX = OLED_OFFSET_X + (OLED_W - lineW) / 2;
      if (lineW > 0) dm::drawHLine(lineX, midY, lineW);
      if (i == P3_FRAMES) {
        dm::drawFilledCircle(OLED_OFFSET_X + OLED_W / 2, midY, 1);
      }
      dm::endFrame();
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    dm::hardClear();
  }

  ledcDetachPin(RED_PIN);
  ledcDetachPin(GREEN_PIN);
  ledcDetachPin(BLUE_PIN);
  pinMode(RED_PIN, INPUT);
  pinMode(GREEN_PIN, INPUT);
  pinMode(BLUE_PIN, INPUT);

  // Record Runtime before Sleep
  uint32_t duration = (millis() - sessionStartTime) / 1000;
  config.totalLifetimeRuntime += duration;
  saveConfig();

  time_t now;
  time(&now);
  appendSessionLog((uint32_t)now, (uint16_t)duration);

  uint32_t sleepTime = (config.sleepMinutes > 0) ? config.sleepMinutes : 5;
  esp_sleep_enable_timer_wakeup(sleepTime * 60 * 1000000ULL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
  esp_deep_sleep_start();
}

void monitorTask(void *pvParameters) {
  sessionStartTime = millis();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  Serial.println("[SYSTEM] Aether Booting...");

  loadConfig();
  migrateNVS();
  loadTrendHistory();

  // Stealth Mode Check
  if (config.ledEnabled) {
    ledcSetup(RED_CH, 5000, 8);
    ledcAttachPin(RED_PIN, RED_CH);
    ledcSetup(GREEN_CH, 5000, 8);
    ledcAttachPin(GREEN_PIN, GREEN_CH);
    ledcSetup(BLUE_CH, 5000, 8);
    ledcAttachPin(BLUE_PIN, BLUE_CH);
    setLED(0, 10, 20);
  } else {
    // Keep as inputs to minimize current draw
    pinMode(RED_PIN, INPUT);
    pinMode(GREEN_PIN, INPUT);
    pinMode(BLUE_PIN, INPUT);
  }

  // Initialize WiFi radio early
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Load Stats
  preferences.begin("stats", false);
  bootCount = preferences.getInt("boots", 0) + 1;
  measureCount = preferences.getInt("measures", 0);
  preferences.putInt("boots", bootCount);
  preferences.end();

  // Location is now in config
  locationSynced = config.locationSynced;
  locLat = config.lat;
  locLon = config.lon;
  locOffset = config.offset;
  strncpy(locCity, config.city, 15);

  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  vTaskDelay(50);
  digitalWrite(OLED_RST, HIGH);
  oledFound = dm::init(displayMutex);
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

  // Phase 3: boot splash on cold boot only (button-wake stays fast).
  if (oledFound && reason != ESP_SLEEP_WAKEUP_EXT0) {
    dm::bootSplash();
  }

  if (reason == ESP_SLEEP_WAKEUP_EXT0) {
    lastInteractionTime = millis();
    buttonEvent = false; // Clear any wakeup noise
    // Seed menu scroll positions so first animation is smooth.
    menuScrollPos = (float)currentMenuIndex;
    wifiMenuScrollPos = 0.f;
    while (millis() - lastInteractionTime < MENU_TIMEOUT) {
      bool triggerAction = false;
      if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
        if (!longPressTriggered) {
          longPressTriggered = true;
          triggerAction = true;
          setLED(0, 255, 0);
        }
      } else if (!isPressing) {
        setLED(0, 10, 20);
      }

      // Rendering happens in uiTask; monitorTask only mutates state.

      if (triggerAction) {
        // Hand over rendering to the invoked subpage so uiTask doesn't fight it.
        g_menuOwnedByPage = true;
        if (currentState == SS_MENU) {
          if (currentMenuIndex == PAGE_MEASURE)
            runMeasurementFlow("manual");
          else if (currentMenuIndex == PAGE_TIME)
            showTimePage();
          else if (currentMenuIndex == PAGE_WEATHER)
            showWeatherPage();
          else if (currentMenuIndex == PAGE_LOCATE)
            runLocatePage();
          else if (currentMenuIndex == PAGE_LED) {
            toggleLED();
            dm::toast(config.ledEnabled ? "LED ON" : "LED OFF", 900);
          } else if (currentMenuIndex == PAGE_INTERVAL) {
            cycleSleepInterval();
            char buf[16];
            snprintf(buf, sizeof(buf), "SLEEP %dM", config.sleepMinutes);
            dm::toast(buf, 900);
          } else if (currentMenuIndex == PAGE_STATS)
            showStatsPage();
          else if (currentMenuIndex == PAGE_ROOM)
            showRoomPage();
          else if (currentMenuIndex == PAGE_TREND)
            showTrendPage();
          else if (currentMenuIndex == PAGE_PORTAL) {
            currentState = SS_WIFI_MENU;
            currentWiFiMenuIndex = 0;
            wifiMenuScrollPos = 0.f;
          } else if (currentMenuIndex == PAGE_RESET)
            runResetStats();
          else if (currentMenuIndex == PAGE_SLEEP)
            enterDeepSleep();
        } else if (currentState == SS_WIFI_MENU) {
          if (currentWiFiMenuIndex == WF_PORTAL)
            runWiFiPortal();
          else if (currentWiFiMenuIndex == WF_SELECT)
            showSavedWiFi();
          else if (currentWiFiMenuIndex == WF_CLEAR)
            deleteSavedWiFi();
          else if (currentWiFiMenuIndex == WF_BACK) {
            currentState = SS_MENU;
            menuScrollPos = (float)currentMenuIndex;
          }
        }
        g_menuOwnedByPage = false;
        lastInteractionTime = millis();
      }

      if (buttonEvent) {
        buttonEvent = false;
        if (currentState == SS_MENU) {
          int oldIdx = currentMenuIndex;
          currentMenuIndex = (currentMenuIndex + 1) % TOTAL_MENU_ITEMS;
          // Smooth scroll (shortest-path). +1 forward each press.
          float delta = shortestSignedDelta(oldIdx, currentMenuIndex, TOTAL_MENU_ITEMS);
          menuTween.start_(menuScrollPos, menuScrollPos + delta, 180);
        } else if (currentState == SS_WIFI_MENU) {
          int oldIdx = currentWiFiMenuIndex;
          currentWiFiMenuIndex =
              (currentWiFiMenuIndex + 1) % TOTAL_WIFI_MENU_ITEMS;
          float delta = shortestSignedDelta(oldIdx, currentWiFiMenuIndex,
                                            TOTAL_WIFI_MENU_ITEMS);
          wifiMenuTween.start_(wifiMenuScrollPos, wifiMenuScrollPos + delta, 180);
        }
        lastInteractionTime = millis();
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  } else {
    // Auto-measure after timer wake: uiTask would otherwise render the
    // cover-flow menu underneath the measurement screens. Take ownership.
    g_menuOwnedByPage = true;
    runMeasurementFlow("auto");
    g_menuOwnedByPage = false;
  }
  enterDeepSleep();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, handleButtonInterrupt, CHANGE);

  displayMutex = xSemaphoreCreateMutex();

  dht.begin();
  // Note: mpu.begin() uses Wire before RTOS tasks start — safe (no contention yet).
  if (mpu.begin())
    mpuFound = true;

  ledcSetup(RED_CH, 5000, 8);
  ledcSetup(GREEN_CH, 5000, 8);
  ledcSetup(BLUE_CH, 5000, 8);
  // Attachment is now conditional in monitorTask

  xTaskCreatePinnedToCore(uiTask, "UI", 4096, NULL, 1, NULL, 1);
  // Lowered to 12KB to save heap for SSL (Supabase)
  xTaskCreatePinnedToCore(monitorTask, "Monitor", 12288, NULL, 2, NULL, 1);
}
void loop() {}
