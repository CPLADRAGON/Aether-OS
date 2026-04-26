#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>
#include <driver/rtc_io.h>
#include <Wire.h>
#include <time.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "secrets.h"

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

enum SystemState { SS_MENU, SS_CONNECTING, SS_SCANNING, SS_SYNCING, SS_LOCATING, SS_CLOCK, SS_WEATHER, SS_SLEEPING, SS_STATS, SS_RESET, SS_PORTAL, SS_WIFI_MENU };
volatile SystemState currentState = SS_MENU;
String uiLine1 = "", uiLine2 = "", uiLine3 = "";
const uint8_t* uiIcon = NULL;

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

WiFiSnapshot currentSnapshot;
DeviceConfig config;
LogQueue logQueue;
uint32_t sessionStartTime = 0; // Relative (millis) or Absolute (UTC)
bool timeSynced = false;

uint32_t calculateCRC(WiFiSnapshot* s) {
  uint32_t originalCrc = s->crc;
  s->crc = 0;
  uint32_t crc = 0; 
  uint8_t* p = (uint8_t*)s;
  for(int i=0; i < sizeof(WiFiSnapshot); i++) crc += p[i];
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
  size_t len = preferences.getBytes("snap", &currentSnapshot, sizeof(WiFiSnapshot));
  preferences.end();
  if (len != sizeof(WiFiSnapshot)) return false;
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
    for (int i = 0; i < 9; i++) logQueue.logs[i] = logQueue.logs[i+1];
    logQueue.logs[9] = {start, dur};
  }
  preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
  preferences.end();
  Serial.printf("[NVS] Log added: %ds. Total in queue: %d\n", dur, logQueue.count);
}

void clearLogQueue() {
  preferences.begin("logs_v2", false);
  logQueue.count = 0;
  preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
  preferences.end();
  Serial.println("[NVS] Log queue cleared.");
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
    preferences.remove("locSync"); preferences.remove("lat");
    preferences.remove("lon"); preferences.remove("offset");
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

// --- OLED Config (64x48 visible window) ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64
#define OLED_W 64
#define OLED_H 48
#define OLED_OFFSET_X 32    
#define OLED_OFFSET_Y 16 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const unsigned char icon_wifi[] PROGMEM = { 0x3C, 0x7E, 0xC3, 0x00, 0x3C, 0x42, 0x00, 0x18 };
const unsigned char icon_cloud[] PROGMEM = { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x24, 0x24 }; // Syncing
const unsigned char icon_pin[] PROGMEM = { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x00, 0x00 };   // Location
const unsigned char icon_scan[] PROGMEM = { 0xFF, 0x81, 0xBD, 0xA5, 0xA5, 0xBD, 0x81, 0xFF }; // Sensor Scan

// --- Menu Configuration ---
enum MenuPage { PAGE_MEASURE, PAGE_TIME, PAGE_WEATHER, PAGE_LOCATE, PAGE_LED, PAGE_INTERVAL, PAGE_STATS, PAGE_PORTAL, PAGE_RESET, PAGE_SLEEP };
const int TOTAL_MENU_ITEMS = 10;
const char* menuItems[] = {"MEASURE", "TIME", "WEATHER", "LOCATE", "LED", "INTERVAL", "STATS", "WIFI MENU", "RESET STATS", "DEEP SLEEP"};

enum WiFiMenuPage { WF_PORTAL, WF_SELECT, WF_CLEAR, WF_BACK };
const char* wifiMenuItems[] = {"PORTAL", "SET TARGET", "CLEAR", "BACK"};
const int TOTAL_WIFI_MENU_ITEMS = 4;
int currentWiFiMenuIndex = 0;
int wifiPriority = 0; // 0: Auto (Cycle 5), 1: Fixed (Selected Slot Only)
const char* prioLabels[] = {"AUTO", "FIXED"};

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
        buttonEvent = true; // Only trigger event for short press if long press wasn't already triggered
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

void setLED(int r, int g, int b) {
  if (!config.ledEnabled && (r > 0 || g > 0 || b > 0)) {
    // Only allow brief flash if disabled, or just keep off
    ledcWrite(RED_CH, 0); ledcWrite(GREEN_CH, 0); ledcWrite(BLUE_CH, 0);
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
  if (config.sleepMinutes == 5) config.sleepMinutes = 15;
  else if (config.sleepMinutes == 15) config.sleepMinutes = 30;
  else if (config.sleepMinutes == 30) config.sleepMinutes = 60;
  else config.sleepMinutes = 5;
  saveConfig();
}

// Helper to wait while checking for ISR button events
bool waitWithButtonPoll(unsigned long ms) {
  buttonEvent = false; // Reset before waiting
  unsigned long start = millis();
  while(millis() - start < ms) {
    if (buttonEvent) {
      buttonEvent = false;
      lastInteractionTime = millis();
      return true;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  return false;
}

void drawPulsingPower(int frame) {
  int cx = OLED_OFFSET_X + 32;
  int cy = OLED_OFFSET_Y + 28;
  float pulse = (sin(frame * 0.2) + 1.0) / 2.0; // 0.0 to 1.0
  int r = 8 + (int)(pulse * 4);
  
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.fillRect(cx - 3, cy - r - 2, 7, 5, SSD1306_BLACK); // Gap
  display.drawLine(cx, cy - r + 1, cx, cy - 2, SSD1306_WHITE); // Stem
}

void uiTask(void *pvParameters) {
  int frame = 0;
  const char* loader = "/|-\\";
  while(1) {
    if (currentState == SS_CONNECTING || currentState == SS_SYNCING || currentState == SS_LOCATING || currentState == SS_SCANNING || currentState == SS_PORTAL) {
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50))) {
        display.clearDisplay();
        // Header
        display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(OLED_OFFSET_X + 4, OLED_OFFSET_Y + 1);
        
        String head = "WAITING";
        if (currentState == SS_CONNECTING) head = "WIFI...";
        else if (currentState == SS_SYNCING) head = "CLOUD";
        else if (currentState == SS_LOCATING) head = "GEO-IP";
        else if (currentState == SS_SCANNING) head = "SCANNING";
        else if (currentState == SS_PORTAL) head = "PORTAL";
        display.print(head);

        if (currentState == SS_PORTAL) {
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 14); // Shifted for zero-clipping
          display.print("CONNECT TO:");
          display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 26);
          display.print("AETHER_CFG");
          display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 38);
          display.print("192.168.4.1");
        } else if (currentState == SS_WIFI_MENU) {
          // WiFi Sub-Menu Rendering
          display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, 10, SSD1306_WHITE);
          display.setTextColor(SSD1306_BLACK);
          display.setCursor(OLED_OFFSET_X + 4, OLED_OFFSET_Y + 1);
          display.print("WIFI CONFIG");
          
          display.setTextColor(SSD1306_WHITE);
          for(int i = 0; i < TOTAL_WIFI_MENU_ITEMS; i++) {
            if (i == currentWiFiMenuIndex) {
              display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y + 14 + (i * 10), OLED_W, 10, SSD1306_WHITE);
              display.setTextColor(SSD1306_BLACK);
            } else {
              display.setTextColor(SSD1306_WHITE);
            }
            display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 15 + (i * 10));
            display.print(wifiMenuItems[i]);
          }
        } else {
          // Progress Layout with Spinner
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 14);
          
          String head = "WAITING";
          if (currentState == SS_CONNECTING) head = "WIFI...";
          else if (currentState == SS_SYNCING) head = "CLOUD";
          else if (currentState == SS_LOCATING) head = "GEO-IP";
          else if (currentState == SS_SCANNING) head = "SCANNING";
          else if (currentState == SS_PORTAL) head = "PORTAL";
          
          display.print((uiLine1 == "") ? head : uiLine1);
          
          display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 28);
          display.print(uiLine2);
          
          // Mechanical Spinner (Size 2, Bottom Right)
          display.setTextSize(2);
          display.setCursor(OLED_OFFSET_X + 46, OLED_OFFSET_Y + 30);
          display.print(loader[frame % 4]);
          display.setTextSize(1);
          
          if (uiLine3 != "") {
            display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 40);
            display.print(uiLine3);
          }
        }
        
        display.display();
        xSemaphoreGive(displayMutex);
        frame++;
      }
    }
    vTaskDelay(150 / portTICK_PERIOD_MS);
  }
}

void sendLog(String msg, String level = "INFO") {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/device_logs";
  if (http.begin(client, url)) {
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Content-Type", "application/json");
    JsonDocument doc;
    doc["message"] = msg; doc["level"] = level;
    String body; serializeJson(doc, body);
    http.POST(body); http.end();
  }
}

void updateOLED(String header, String line1, String line2, String line3 = "", const uint8_t* icon = NULL) {
  if (!oledFound) return;
  uiLine1 = line1; uiLine2 = line2; uiLine3 = line3; uiIcon = icon;
  
  if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
    display.clearDisplay();
    display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(OLED_OFFSET_X + 4, OLED_OFFSET_Y + 1);
    display.print(header);
    
    if (icon != NULL) {
      display.drawBitmap(OLED_OFFSET_X + OLED_W - 10, OLED_OFFSET_Y + 1, icon, 8, 8, SSD1306_BLACK);
    }

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(OLED_OFFSET_X + 4, OLED_OFFSET_Y + 14); display.print(line1);
    display.setCursor(OLED_OFFSET_X + 4, OLED_OFFSET_Y + 24); display.print(line2);
    if (line3 != "") {
      display.setCursor(OLED_OFFSET_X + 4, OLED_OFFSET_Y + 34); display.print(line3);
    }
    display.display();
    xSemaphoreGive(displayMutex);
  }
}

bool validateIPReady() {
  if (WiFi.status() != WL_CONNECTED) return false;
  IPAddress out;
  // Aggressive 750ms DNS probe on Supabase host
  uint32_t start = millis();
  while(millis() - start < 750) {
    if (WiFi.hostByName("tsawczfmlqrmtyojkkne.supabase.co", out)) return true;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
  return false;
}

// Return codes: 0=OK, 1=UserAbort, 2=NoSSID, 3=AuthFail, 4=Timeout
int tryConnect(const char* ssid, const char* pass) {
  String sStr = String(ssid); sStr.trim();
  String pStr = String(pass); pStr.trim();
  if (sStr.length() == 0) return 4;
  
  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) {
    display.clearDisplay();
    display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 1);
    display.print("CONNECTING");
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 14);
    display.print(sStr.substring(0,9));
    display.display();
    xSemaphoreGive(displayMutex);
  }

  WiFi.disconnect();
  vTaskDelay(200 / portTICK_PERIOD_MS);
  
  bool snapshotValid = loadWiFiSnapshot();
  bool isFastTrackSSID = (snapshotValid && sStr == String(currentSnapshot.ssid));
  bool attemptFast = isFastTrackSSID;

  if (attemptFast) {
    WiFi.config(IPAddress(currentSnapshot.ip), IPAddress(currentSnapshot.gateway), 
                IPAddress(currentSnapshot.subnet), IPAddress(currentSnapshot.dns));
    WiFi.begin(sStr.c_str(), pStr.c_str(), currentSnapshot.channel, currentSnapshot.bssid);
  } else {
    WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
    WiFi.begin(sStr.c_str(), pStr.c_str());
  }

  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.printf("[WIFI] Target: %s\n", sStr.c_str());
  uiLine2 = sStr.substring(0,10); // Show on OLED via uiTask spinner
  
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
      WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
      WiFi.begin(sStr.c_str(), pStr.c_str());
      attemptFast = false;
    }

    if (i > 60) {
      if (status == WL_NO_SSID_AVAIL) return 2;
      if (status == WL_CONNECT_FAILED) return 3;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  WiFi.disconnect();
  return 4; // Timeout
}

bool ensureWiFi(bool force = false) {
  if (!force && WiFi.status() == WL_CONNECTED) return true;
  
  currentState = SS_CONNECTING;
  uiLine1 = "WIFI..."; uiLine2 = ""; uiLine3 = ""; 
  
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
      if (buttonEvent) { buttonEvent = false; break; }
      preferences.begin("aether_wifi", true);
      s = preferences.getString(("s" + String(i)).c_str(), "");
      p = preferences.getString(("p" + String(i)).c_str(), "");
      preferences.end();
      if (s.length() > 0) {
        res = tryConnect(s.c_str(), p.c_str());
        if (res == 0) {
          if (validateIPReady()) return true;
        }
        if (res == 1) break;
      }
    }
  } else {
    res = tryConnect(s.c_str(), p.c_str());
    if (res == 0) {
      if (validateIPReady()) return true;
    }
  }

  // Handle Errors Verbously
  currentState = SS_MENU; 
  if (res != 0 && res != 1) {
    String err = "TIMEOUT";
    if (res == 2) err = "NO SIGNAL";
    if (res == 3) err = "AUTH FAIL";
    updateOLED("WIFI", "FAILED", err, "TRY AGAIN");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
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
    Serial.print("[PORTAL] AP Started. SSID: AETHER_CONFIG, Pass: aether123, IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[PORTAL] AP Start Failed!");
  }
  
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println("[PORTAL] DNS Server Started.");
  
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>AETHER | WiFi Setup</title><style>"
                "body{margin:0;padding:0;background:#0a0a0a;color:#fff;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;color-scheme:dark}"
                ".card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.1);padding:40px;border-radius:24px;width:90%;max-width:340px;box-shadow:0 20px 50px rgba(0,0,0,0.5);text-align:center}"
                "h1{font-size:24px;font-weight:700;letter-spacing:4px;margin:0 0 8px;background:linear-gradient(90deg,#00bcd4,#00acc1);-webkit-background-clip:text;-webkit-text-fill-color:transparent}"
                "p{color:rgba(255,255,255,0.5);font-size:14px;margin-bottom:32px}"
                "select,input{width:100%;background:#1a1a1a;border:1px solid rgba(255,255,255,0.1);border-radius:12px;padding:14px;margin-bottom:16px;color:#fff;font-size:16px;box-sizing:border-box;appearance:none;transition:all 0.3s}"
                "select:focus,input:focus{outline:none;border-color:#00bcd4;background:rgba(255,255,255,0.1)}"
                "option{background:#1a1a1a;color:#fff}"
                "button{width:100%;background:#00bcd4;color:#000;border:none;border-radius:12px;padding:16px;font-size:16px;font-weight:700;cursor:pointer;transition:transform 0.2s,background 0.3s;margin-top:8px}"
                "button:active{transform:scale(0.98);background:#00acc1}"
                "footer{margin-top:32px;font-size:10px;color:rgba(255,255,255,0.3);letter-spacing:1px}"
                "</style></head><body><div class='card'><h1>AETHER</h1><p>WIFI PROVISIONING</p>"
                "<form action='/save' method='POST'>"
                "<select name='slot'>"
                "<option value='1'>Profile Slot 1</option><option value='2'>Profile Slot 2</option><option value='3'>Profile Slot 3</option>"
                "<option value='4'>Profile Slot 4</option><option value='5'>Profile Slot 5</option></select>"
                "<input name='s' placeholder='Network SSID' required><input name='p' type='password' placeholder='Password'>"
                "<button type='submit'>STORE WIFI</button></form><footer>v2.3.1 // BANK_5_SLOTS_ACTIVE</footer></div></body></html>";
  
  server.on("/", HTTP_GET, [&html]() {
    Serial.println("[PORTAL] Client requested root page.");
    server.send(200, "text/html", html);
  });
  
  server.on("/save", HTTP_POST, []() {
    String s = server.arg("s"); s.trim();
    String p = server.arg("p"); p.trim();
    String slot = server.arg("slot");
    if (slot == "") slot = "1";
    
    Serial.printf("[PORTAL] Saving to Slot %s: %s\n", slot.c_str(), s.c_str());
    if (s.length() > 0) {
      Preferences pStore;
      pStore.begin("aether_wifi", false);
      pStore.putString(("s" + slot).c_str(), s);
      pStore.putString(("p" + slot).c_str(), p);
      pStore.end();
      server.send(200, "text/html", "<h1>SAVED</h1><p>Slot " + slot + " updated. Rebooting...</p>");
      delay(1000);
      ESP.restart();
    }
    server.send(400, "text/plain", "SSID REQUIRED");
  });
  
  // Captive Portal Redirects (Standard for Apple, Android, Windows)
  server.on("/generate_204", HTTP_GET, [&html]() { server.send(200, "text/html", html); });
  server.on("/fwlink", HTTP_GET, [&html]() { server.send(200, "text/html", html); });
  server.on("/hotspot-detect.html", HTTP_GET, [&html]() { server.send(200, "text/html", html); });
  server.on("/canonical.html", HTTP_GET, [&html]() { server.send(200, "text/html", html); });
  server.on("/success.txt", HTTP_GET, []() { server.send(200, "text/plain", "success"); });

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

    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) {
      display.clearDisplay();
      display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 1);
      
      String head = "SLOT " + String(currentSlot);
      if (currentSlot == primSlot) head += " [*]";
      display.print(head);
      
      display.setTextColor(SSD1306_WHITE);
      if (viewMode == 0) {
        display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 14);
        display.print("SSID:");
        display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 24);
        display.print(s.substring(0, 9));
        display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 38);
        display.print("[HOLD] SET");
      } else {
        display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 14);
        display.print("P:" + p.substring(0, 9));
        display.setCursor(OLED_OFFSET_X + 0, OLED_OFFSET_Y + 38);
        display.print("[CLICK] BK");
      }
      display.display();
      xSemaphoreGive(displayMutex);
    }

    if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
      if (!longPressTriggered) {
        longPressTriggered = true;
        // SET AS PRIMARY
        preferences.begin("aether", false);
        preferences.putInt("primSlot", currentSlot);
        preferences.end();
        primSlot = currentSlot; // Update indicator
        updateOLED("WIFI", "PRIMARY", "SET TO", "SLOT " + String(currentSlot));
        vTaskDelay(1200 / portTICK_PERIOD_MS);
        break; 
      }
    }
    
    if (buttonEvent) {
      buttonEvent = false;
      if (viewMode == 1) {
        viewMode = 0;
      } else {
        currentSlot++;
        if (currentSlot > 5) currentSlot = 1;
      }
      lastInteractionTime = millis();
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void deleteSavedWiFi() {
  preferences.begin("aether_wifi", false);
  preferences.clear();
  preferences.end();
  updateOLED("WIFI", "CREDENTIALS", "CLEARED", "REBOOTING...");
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  ESP.restart();
}

void runLocatePage() {
  if (!ensureWiFi()) return;
  currentState = SS_LOCATING;
  uiLine1 = "FETCHING IP...";
  
  WiFiClient client; HTTPClient http;
  String url = "http://ip-api.com/json/?fields=status,city,lat,lon,offset";
  if (http.begin(client, url)) {
    int code = http.GET();
    currentState = SS_MENU;
    if (code == 200) {
      JsonDocument doc; deserializeJson(doc, http.getString());
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
        updateOLED("LOCATE", "FOUND:", cleanCity, "SAVED", icon_pin);
        waitWithButtonPoll(3000);
      } else {
        updateOLED("LOCATE", "API ERR", "FAILED");
        waitWithButtonPoll(3000);
      }
    } else {
      updateOLED("LOCATE", "HTTP ERR", "CODE:" + String(code));
      waitWithButtonPoll(3000);
    }
    http.end();
  }
  currentState = SS_MENU;
}

void showTimePage() {
  if (!locationSynced) {
    updateOLED("CLOCK", "LOCATION", "REQ FIRST", "RUN LOCATE");
    waitWithButtonPoll(3000);
    return;
  }

  if (!ensureWiFi()) return;
  currentState = SS_CONNECTING;
  uiLine1 = "NTP SYNC...";
  
  long tzHours = locOffset / 3600;
  String tzString = "UTC" + String(tzHours > 0 ? "-" : "+") + String(abs(tzHours));
  configTime(locOffset, 0, "pool.ntp.org");
  setenv("TZ", tzString.c_str(), 1); tzset();
  
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
      char tStr[10], dStr[12], dayStr[16];
      strftime(tStr, 10, "%H:%M:%S", &tinfo);
      strftime(dStr, 12, "%d/%m/%Y", &tinfo);
      strftime(dayStr, 16, "%A", &tinfo);
      String day = String(dayStr); day.toUpperCase();
      updateOLED("CLOCK", tStr, dStr, day);
    }
    if (waitWithButtonPoll(500)) break; // Cancel on button press
  }
}

void showWeatherPage() {
  if (!locationSynced) {
    updateOLED("WEATHER", "LOCATION", "REQ FIRST", "RUN LOCATE");
    waitWithButtonPoll(3000);
    return;
  }

  if (!ensureWiFi()) return;
  currentState = SS_SYNCING;
  uiLine1 = "FETCHING";
  uiLine2 = "DATA...";
  uiLine3 = "";
  
  #ifdef WEATHER_API_KEY
    WiFiClient client; HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(locLat, 4) + "&lon=" + String(locLon, 4) + "&units=metric&appid=" + String(WEATHER_API_KEY);
    if (http.begin(client, url)) {
      int code = http.GET();
      currentState = SS_MENU;
      if (code == 200) {
        JsonDocument doc; deserializeJson(doc, http.getString());
        float temp = doc["main"]["temp"].as<float>();
        int hum = doc["main"]["humidity"].as<int>();
        String desc = doc["weather"][0]["main"].as<String>();
        
        updateOLED("WEATHER", String(temp, 1) + " C", desc, "HUM: " + String(hum) + "%");
      } else { 
        updateOLED("WEATHER", "API ERR", "CODE:" + String(code)); 
      }
      http.end();
    }
  #else
    updateOLED("WEATHER", "30.5 C", "SUNNY", "KEY REQ");
  #endif
  waitWithButtonPoll(8000); // Wait 8s, cancel on button press
}

void runMeasurementFlow(String trigger) {
  if (trigger == "manual") {
    updateOLED("AETHER", "INITIATING", "SCAN...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  float totalT = 0, totalH = 0, totalA = 0; int totalL = 0, validCount = 0;
  String sampleLog = "Scans: ";
  
  for (int i = 0; i < 5; i++) {
    // Check for long press to exit instantly
    unsigned long stepStart = millis();
    while(millis() - stepStart < 2500) {
      if (isPressing && (millis() - isrPressStart > LONG_PRESS_MS)) {
        updateOLED("AETHER", "EXITING...", "STOPPED");
        vTaskDelay(800 / portTICK_PERIOD_MS);
        return;
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    
    setLED(i * 40, 200, 0);
    float t = dht.readTemperature(); float h = dht.readHumidity(); int l = analogRead(LDR_PIN);
    float acc = 0;
    if (mpuFound) {
      sensors_event_t ae, ge, te;
      if (mpu.getEvent(&ae, &ge, &te)) acc = sqrt(sq(ae.acceleration.x) + sq(ae.acceleration.y) + sq(ae.acceleration.z));
    }
    
    String headerStr = "SCAN " + String(i + 1) + "/5";
    if (!isnan(t) && !isnan(h) && h <= 100.0 && t < 60.0) {
      totalT += t; totalH += h; totalL += l; totalA += acc; validCount++;
      sampleLog += "[T:" + String(t, 1) + " H:" + String(h, 0) + "] ";
      updateOLED(headerStr, "T:" + String(t, 1) + "C", "H:" + String(h, 0) + "%", "L:" + String(l));
    } else {
      sampleLog += "[ERR] ";
      updateOLED(headerStr, "SENSOR", "GLITCH", "SKIPPED");
    }
  }

  if (validCount > 0) {
    uiLine1 = "LINKING...";
    if (!ensureWiFi(true)) return; 
    
    currentState = SS_SYNCING;
    uiLine1 = "SYNCING...";
    Serial.printf("[SYSTEM] Free Heap: %d bytes\n", ESP.getFreeHeap());

    
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    http.setReuse(true); 

    // Task 1: Debug Log
    if (http.begin(client, String(SUPABASE_URL) + "/rest/v1/device_logs")) {
      http.addHeader("apikey", SUPABASE_KEY);
      http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
      http.addHeader("Content-Type", "application/json");
      JsonDocument logDoc; logDoc["message"] = sampleLog; logDoc["level"] = "DEBUG";
      String logBody; serializeJson(logDoc, logBody);
      http.POST(logBody);
    }

    // Task 2: Data Upload
    if (http.begin(client, String(SUPABASE_URL) + "/rest/v1/room_readings")) {
      http.addHeader("apikey", SUPABASE_KEY); 
      http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY)); 
      http.addHeader("Content-Type", "application/json");
      JsonDocument doc; 
      doc["temperature"] = totalT / validCount; doc["humidity"] = totalH / validCount;
      doc["ldr_value"] = totalL / validCount; doc["accel_total"] = totalA / validCount;
      doc["trigger_source"] = trigger; doc["battery_v"] = 3.3;
      String body; serializeJson(doc, body);
      int code = http.POST(body);
      if (code >= 200 && code < 300) { 
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
          if (http.begin(client, String(SUPABASE_URL) + "/rest/v1/device_sessions")) {
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
            String sessionBody; serializeJson(sessionDoc, sessionBody);
            int code = http.POST(sessionBody);
            if (code >= 200 && code < 300) {
              logQueue.count = 0;
              preferences.begin("logs_v2", false);
              preferences.putBytes("queue", &logQueue, sizeof(LogQueue));
              preferences.end();
            }
          }
        }
        
        currentState = SS_MENU;
        updateOLED("CLOUD", "SYNCED", "SUCCESS", "SAVED", icon_cloud);
        vTaskDelay(1500 / portTICK_PERIOD_MS);
      }
      http.end();
    }
    currentState = SS_MENU;
  } else {
    updateOLED("ERROR", "SENSOR", "FAILED", "NO DATA");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void runResetStats() {
  updateOLED("RESET", "CLEARING", "HISTORY...");
  bootCount = 0;
  measureCount = 0;
  preferences.begin("stats", false);
  preferences.putInt("boots", 0);
  preferences.putInt("measures", 0);
  preferences.end();
  vTaskDelay(1500 / portTICK_PERIOD_MS);
  updateOLED("RESET", "STATS", "CLEARED");
  vTaskDelay(1500 / portTICK_PERIOD_MS);
}

void showStatsPage() {
  uint32_t totalMin = config.totalLifetimeRuntime / 60;
  String upStr = "UP:" + String(totalMin) + "m";
  if (totalMin > 1440) upStr = "UP:" + String(totalMin / 60) + "h";
  
  updateOLED("STATS", "MES:" + String(measureCount), "BOT:" + String(bootCount), upStr);
  waitWithButtonPoll(5000); // Cancel on button press
}

void drawMenu() {
  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) {
    display.clearDisplay();
    
    // Header Logic (Universal for both menus)
    display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    
    String headStr = "";
    if (currentState == SS_WIFI_MENU) {
      headStr = "WIFI CFG";
    } else {
      if (locationSynced) headStr = String(locCity).substring(0, 3);
      else headStr = "Loc:-";
    }
    headStr.toUpperCase();
    display.setCursor(OLED_OFFSET_X + 2, OLED_OFFSET_Y + 1);
    display.print(headStr);
    
    // WiFi Status Icon
    if (WiFi.status() == WL_CONNECTED) {
      display.drawBitmap(OLED_OFFSET_X + OLED_W - 10, OLED_OFFSET_Y + 1, icon_wifi, 8, 8, SSD1306_BLACK);
    } else {
      display.setCursor(OLED_OFFSET_X + OLED_W - 8, OLED_OFFSET_Y + 1);
      display.print("x");
    }

    if (currentState == SS_WIFI_MENU) {
      int startIdx = currentWiFiMenuIndex - (VISIBLE_MENU_ITEMS / 2);
      if (startIdx < 0) startIdx = 0;
      if (startIdx > TOTAL_WIFI_MENU_ITEMS - VISIBLE_MENU_ITEMS) startIdx = TOTAL_WIFI_MENU_ITEMS - VISIBLE_MENU_ITEMS;

      for(int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
        int idx = startIdx + i;
        int y = OLED_OFFSET_Y + 14 + (i * 11);
        if (idx == currentWiFiMenuIndex) {
          display.fillRect(OLED_OFFSET_X, y - 1, OLED_W, 11, SSD1306_WHITE);
          display.setTextColor(SSD1306_BLACK);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(OLED_OFFSET_X + 2, y);
        
        display.print(wifiMenuItems[idx]);
      }
    } else {
      int startItem = currentMenuIndex - (VISIBLE_MENU_ITEMS / 2);
      if (startItem < 0) startItem = 0;
      if (startItem > TOTAL_MENU_ITEMS - VISIBLE_MENU_ITEMS) startItem = TOTAL_MENU_ITEMS - VISIBLE_MENU_ITEMS;

      for(int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
        int idx = startItem + i;
        int y = OLED_OFFSET_Y + 14 + (i * 11);
        if (idx == currentMenuIndex) {
          display.fillRect(OLED_OFFSET_X, y - 1, OLED_W, 11, SSD1306_WHITE);
          display.setTextColor(SSD1306_BLACK);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(OLED_OFFSET_X + 2, y);
        if (idx == PAGE_LED) {
          display.print("LED: ");
          display.print(config.ledEnabled ? "ON" : "OFF");
        } else if (idx == PAGE_INTERVAL) {
          display.print("SLEEP: ");
          if (config.sleepMinutes < 60) {
            display.print(config.sleepMinutes);
            display.print("M");
          } else {
            display.print("1H");
          }
        } else {
          display.print(menuItems[idx]);
        }
      }
    }
    
    // Auto Deep Sleep Timer Bar (Bottom)
    unsigned long elapsed = millis() - lastInteractionTime;
    if (elapsed < MENU_TIMEOUT) {
      int barWidth = map(elapsed, 0, MENU_TIMEOUT, OLED_W - 4, 0);
      display.fillRect(OLED_OFFSET_X + 2, OLED_OFFSET_Y + OLED_H - 3, barWidth, 2, SSD1306_WHITE);
    }

    display.display();
    xSemaphoreGive(displayMutex);
  }
}

void enterDeepSleep() {
  currentState = SS_SLEEPING;
  if (oledFound) {
    if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
      for (int i = 0; i <= 10; i++) {
        display.clearDisplay();
        
        // Header
        display.fillRect(OLED_OFFSET_X, OLED_OFFSET_Y, OLED_W, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(OLED_OFFSET_X + 4, OLED_OFFSET_Y + 1);
        display.print("SLEEPING...");
        
        // Power Icon
        int cx = OLED_OFFSET_X + 32;
        int cy = OLED_OFFSET_Y + 28;
        display.drawCircle(cx, cy, 10, SSD1306_WHITE);
        display.fillRect(cx - 3, cy - 13, 7, 6, SSD1306_BLACK); 
        display.drawLine(cx, cy - 11, cx, cy - 2, SSD1306_WHITE); 
        
        // Progress Bar
        int barW = map(i, 0, 10, 0, 40);
        display.drawRect(OLED_OFFSET_X + 12, OLED_OFFSET_Y + 42, 40, 3, SSD1306_WHITE);
        display.fillRect(OLED_OFFSET_X + 12, OLED_OFFSET_Y + 42, barW, 3, SSD1306_WHITE);
        
        display.display();
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
      display.clearDisplay();
      display.display();
      xSemaphoreGive(displayMutex);
    }
  }
  
  ledcDetachPin(RED_PIN); ledcDetachPin(GREEN_PIN); ledcDetachPin(BLUE_PIN);
  pinMode(RED_PIN, INPUT); pinMode(GREEN_PIN, INPUT); pinMode(BLUE_PIN, INPUT);
  
  // Record Runtime before Sleep
  uint32_t duration = (millis() - sessionStartTime) / 1000;
  config.totalLifetimeRuntime += duration;
  saveConfig();
  
  time_t now; time(&now);
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
  
  // Stealth Mode Check
  if (config.ledEnabled) {
    ledcSetup(RED_CH, 5000, 8); ledcAttachPin(RED_PIN, RED_CH);
    ledcSetup(GREEN_CH, 5000, 8); ledcAttachPin(GREEN_PIN, GREEN_CH);
    ledcSetup(BLUE_CH, 5000, 8); ledcAttachPin(BLUE_PIN, BLUE_CH);
    setLED(0, 10, 20); 
  } else {
    // Keep as inputs to minimize current draw
    pinMode(RED_PIN, INPUT); pinMode(GREEN_PIN, INPUT); pinMode(BLUE_PIN, INPUT);
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

  pinMode(OLED_RST, OUTPUT); digitalWrite(OLED_RST, LOW); vTaskDelay(50); digitalWrite(OLED_RST, HIGH);
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) oledFound = true;
  else if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) oledFound = true;
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_EXT0) {
    lastInteractionTime = millis();
    buttonEvent = false; // Clear any wakeup noise
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
      
      drawMenu();

      if (triggerAction) {
        if (currentState == SS_MENU) {
          if (currentMenuIndex == PAGE_MEASURE) runMeasurementFlow("manual");
          else if (currentMenuIndex == PAGE_TIME) showTimePage();
          else if (currentMenuIndex == PAGE_WEATHER) showWeatherPage();
          else if (currentMenuIndex == PAGE_LOCATE) runLocatePage();
          else if (currentMenuIndex == PAGE_LED) toggleLED();
          else if (currentMenuIndex == PAGE_INTERVAL) cycleSleepInterval();
          else if (currentMenuIndex == PAGE_STATS) showStatsPage();
          else if (currentMenuIndex == PAGE_PORTAL) { currentState = SS_WIFI_MENU; currentWiFiMenuIndex = 0; }
          else if (currentMenuIndex == PAGE_RESET) runResetStats();
          else if (currentMenuIndex == PAGE_SLEEP) enterDeepSleep();
        } else if (currentState == SS_WIFI_MENU) {
          if (currentWiFiMenuIndex == WF_PORTAL) runWiFiPortal();
          else if (currentWiFiMenuIndex == WF_SELECT) showSavedWiFi();
          else if (currentWiFiMenuIndex == WF_CLEAR) deleteSavedWiFi();
          else if (currentWiFiMenuIndex == WF_BACK) currentState = SS_MENU;
        }
        lastInteractionTime = millis();
      }

      if (buttonEvent) {
        buttonEvent = false; 
        if (currentState == SS_MENU) {
          currentMenuIndex = (currentMenuIndex + 1) % TOTAL_MENU_ITEMS;
        } else if (currentState == SS_WIFI_MENU) {
          currentWiFiMenuIndex = (currentWiFiMenuIndex + 1) % TOTAL_WIFI_MENU_ITEMS;
        }
        lastInteractionTime = millis();
      }
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  } else { runMeasurementFlow("auto"); }
  enterDeepSleep();
}

void setup() {
  Serial.begin(115200); Wire.begin(21, 22); 
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, handleButtonInterrupt, CHANGE);
  
  displayMutex = xSemaphoreCreateMutex();
  
  dht.begin();
  if (mpu.begin()) mpuFound = true;

  ledcSetup(RED_CH, 5000, 8);
  ledcSetup(GREEN_CH, 5000, 8);
  ledcSetup(BLUE_CH, 5000, 8);
  // Attachment is now conditional in monitorTask
  
  xTaskCreatePinnedToCore(uiTask, "UI", 4096, NULL, 1, NULL, 1); 
  // Lowered to 12KB to save heap for SSL (Supabase)
  xTaskCreatePinnedToCore(monitorTask, "Monitor", 12288, NULL, 2, NULL, 1); 
}
void loop() {}
