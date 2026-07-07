#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

#include "AppConfig.h"
#include "WebUI.h"
#include "MedLogic.h"

// ===========================================================================
// Globals
// ===========================================================================
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

Chamber chambers[NUM_CHAMBERS];
bool rtcOk = false;

bool     setupMode = true;             // true = AP provisioning, false = STA
String   deviceToken;                  // bearer token for the API
String   deviceId;                     // stable id from the chip MAC
char     wifiSsid[33] = "";
char     wifiPass[65] = "";
long     tzOffset = DEFAULT_TZ_OFFSET;
IPAddress ipAddr;
bool     otaAuthorized = false;        // gate for the HTTP OTA upload
uint32_t bootMillis = 0;

static const char *CONFIG_PATH = "/config.json";
static const char *WIFI_PATH   = "/wifi.json";
static const char *TOKEN_PATH  = "/token.txt";

// ===========================================================================
// Persistence
// ===========================================================================
void saveConfig()
{
    JsonDocument doc;
    doc["tz"] = tzOffset;
    JsonArray arr = doc["chambers"].to<JsonArray>();
    for (int i = 0; i < NUM_CHAMBERS; i++)
    {
        JsonObject o = arr.add<JsonObject>();
        o["label"]  = chambers[i].label;
        o["hour"]   = chambers[i].hour;
        o["minute"] = chambers[i].minute;
        o["enabled"] = chambers[i].enabled;
        o["start"]  = chambers[i].startDate;
        o["end"]    = chambers[i].endDate;
        o["taken"]  = chambers[i].takenCount;
        o["missed"] = chambers[i].missedCount;
        o["last"]   = chambers[i].lastTakenEpoch;
    }
    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f) { Serial.println("save: open failed"); return; }
    serializeJson(doc, f);
    f.close();
}

void applyDefaults()
{
    const uint8_t defHour[NUM_CHAMBERS] = {8, 10, 12, 15, 18, 21};
    for (int i = 0; i < NUM_CHAMBERS; i++)
    {
        snprintf(chambers[i].label, sizeof(chambers[i].label), "Medicine %d", i + 1);
        chambers[i].hour = defHour[i];
        chambers[i].minute = 0;
        chambers[i].enabled = false;
        chambers[i].startDate[0] = 0;
        chambers[i].endDate[0] = 0;
        chambers[i].takenCount = 0;
        chambers[i].missedCount = 0;
        chambers[i].lastTakenEpoch = 0;
    }
}

void loadConfig()
{
    applyDefaults();
    File f = LittleFS.open(CONFIG_PATH, "r");
    if (!f) { saveConfig(); return; }
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();
    tzOffset = doc["tz"] | tzOffset;
    JsonArray arr = doc["chambers"].as<JsonArray>();
    int i = 0;
    for (JsonObject o : arr)
    {
        if (i >= NUM_CHAMBERS) break;
        snprintf(chambers[i].label, sizeof(chambers[i].label), "%s",
                 (const char *)(o["label"] | chambers[i].label));
        chambers[i].hour = o["hour"] | chambers[i].hour;
        chambers[i].minute = o["minute"] | chambers[i].minute;
        chambers[i].enabled = o["enabled"] | chambers[i].enabled;
        snprintf(chambers[i].startDate, sizeof(chambers[i].startDate), "%s",
                 (const char *)(o["start"] | ""));
        snprintf(chambers[i].endDate, sizeof(chambers[i].endDate), "%s",
                 (const char *)(o["end"] | ""));
        chambers[i].takenCount = o["taken"] | 0;
        chambers[i].missedCount = o["missed"] | 0;
        chambers[i].lastTakenEpoch = o["last"] | 0;
        i++;
    }
}

void loadWifi()
{
    File f = LittleFS.open(WIFI_PATH, "r");
    if (!f) return;
    JsonDocument doc;
    if (!deserializeJson(doc, f))
    {
        snprintf(wifiSsid, sizeof(wifiSsid), "%s", (const char *)(doc["ssid"] | ""));
        snprintf(wifiPass, sizeof(wifiPass), "%s", (const char *)(doc["pass"] | ""));
    }
    f.close();
}

void saveWifi(const char *ssid, const char *pass)
{
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    File f = LittleFS.open(WIFI_PATH, "w");
    if (f) { serializeJson(doc, f); f.close(); }
}

void loadOrCreateToken()
{
    File f = LittleFS.open(TOKEN_PATH, "r");
    if (f) { deviceToken = f.readString(); deviceToken.trim(); f.close(); }
    if (deviceToken.length() >= 16) return;

    char b[25];
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 24; i++) b[i] = hex[esp_random() & 0x0F];
    b[24] = 0;
    deviceToken = String(b);
    File w = LittleFS.open(TOKEN_PATH, "w");
    if (w) { w.print(deviceToken); w.close(); }
}

// ===========================================================================
// Reminder state machine (non-blocking)
// ===========================================================================
uint32_t nowEpoch()
{
    return rtcOk ? rtc.now().unixtime() : (millis() / 1000);
}

// "YYYY-MM-DD" -> comparable YYYYMMDD int, or 0 if unset/invalid.
uint32_t dateKey(const char *s)
{
    if (!s || strlen(s) < 10) return 0;
    return (uint32_t)atoi(s) * 10000 + atoi(s + 5) * 100 + atoi(s + 8);
}

// Is today within the chamber's optional [start, end] course window?
bool withinWindow(const Chamber &c, const DateTime &now)
{
    uint32_t today = (uint32_t)now.year() * 10000 + now.month() * 100 + now.day();
    uint32_t st = dateKey(c.startDate), en = dateKey(c.endDate);
    return (st == 0 || today >= st) && (en == 0 || today <= en);
}

void startAlert(int i)
{
    chambers[i].alerting = true;
    chambers[i].taken = false;
    chambers[i].triggerEpoch = nowEpoch();
    Serial.printf("ALERT box %d (%s)\n", i + 1, chambers[i].label);
}

void checkSchedule()
{
    if (!rtcOk) return;
    DateTime now = rtc.now();
    for (int i = 0; i < NUM_CHAMBERS; i++)
    {
        medlogic::MedicineSchedule s = {chambers[i].hour, chambers[i].minute};
        if (chambers[i].enabled && withinWindow(chambers[i], now) &&
            medlogic::shouldTrigger(s, chambers[i].triggered, now.hour(), now.minute()))
        {
            chambers[i].triggered = true;
            startAlert(i);
        }
        if (now.hour() != chambers[i].hour || now.minute() != chambers[i].minute)
            chambers[i].triggered = false;
    }
}

int handleAlerts()
{
    int active = -1;
    bool anyAlert = false;
    uint32_t epoch = nowEpoch();

    for (int i = 0; i < NUM_CHAMBERS; i++)
    {
        if (chambers[i].alerting)
        {
            anyAlert = true;
            if (active < 0) active = i;
            digitalWrite(LED_PINS[i], HIGH);

            if (medlogic::isBoxOpen(digitalRead(REED_PINS[i]), REED_OPEN_LEVEL[i]))
            {
                chambers[i].alerting = false;
                chambers[i].taken = true;
                chambers[i].takenCount++;
                chambers[i].lastTakenEpoch = epoch;
                digitalWrite(LED_PINS[i], LOW);
                Serial.printf("TAKEN box %d\n", i + 1);
                saveConfig();
            }
            else if (epoch - chambers[i].triggerEpoch > ALERT_TIMEOUT_SECONDS)
            {
                chambers[i].alerting = false;
                chambers[i].missedCount++;
                digitalWrite(LED_PINS[i], LOW);
                Serial.printf("MISSED box %d\n", i + 1);
                saveConfig();
            }
        }
        else
        {
            digitalWrite(LED_PINS[i], LOW);
        }
    }

    digitalWrite(BUZZER_PIN, (anyAlert && (millis() / 300) % 2) ? HIGH : LOW);
    return active;
}

void updateLcd(int activeAlert)
{
    static uint32_t last = 0;
    if (millis() - last < 500) return;
    last = millis();

    char line1[17], line2[17];
    if (activeAlert >= 0)
    {
        snprintf(line1, sizeof(line1), "TAKE Box %d", activeAlert + 1);
        snprintf(line2, sizeof(line2), "%-16.16s", chambers[activeAlert].label);
    }
    else if (rtcOk)
    {
        DateTime now = rtc.now();
        medlogic::formatTime(line1, now.hour(), now.minute(), now.second());
        if (setupMode)
            snprintf(line2, sizeof(line2), "AP setup mode");
        else
            snprintf(line2, sizeof(line2), "%-16.16s", ipAddr.toString().c_str());
    }
    else { snprintf(line1, sizeof(line1), "RTC error"); snprintf(line2, sizeof(line2), "check wiring"); }

    lcd.setCursor(0, 0); lcd.print(line1);
    for (int i = strlen(line1); i < 16; i++) lcd.print(' ');
    lcd.setCursor(0, 1); lcd.print(line2);
    for (int i = strlen(line2); i < 16; i++) lcd.print(' ');
}

// ===========================================================================
// HTTP helpers + auth
// ===========================================================================
void addCors()
{
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void sendJson(int code, const String &body)
{
    addCors();
    server.send(code, "application/json", body);
}

bool authed()
{
    String h = server.header("Authorization");
    return h == ("Bearer " + deviceToken);
}

bool requireAuth()
{
    if (authed()) return true;
    sendJson(401, "{\"ok\":false,\"error\":\"unauthorized\"}");
    return false;
}

String epochToStr(uint32_t e)
{
    if (e == 0) return "never";
    DateTime d((uint32_t)e);
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", d.day(), d.month(), d.hour(), d.minute());
    return String(buf);
}

// ===========================================================================
// API v1 handlers
// ===========================================================================
void apiHealth()          // open
{
    sendJson(200, String("{\"ok\":true,\"status\":\"up\",\"fw\":\"") + FW_VERSION + "\"}");
}

void apiInfo()            // open (no secrets)
{
    JsonDocument doc;
    doc["ok"] = true;
    doc["device_id"] = deviceId;
    doc["fw"] = FW_VERSION;
    doc["chambers"] = NUM_CHAMBERS;
    doc["mode"] = setupMode ? "ap" : "sta";
    doc["ip"] = ipAddr.toString();
    doc["mdns"] = String(MDNS_HOST) + ".local";
    doc["uptime_s"] = (millis() - bootMillis) / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["rtc_ok"] = rtcOk;
    String out; serializeJson(doc, out);
    sendJson(200, out);
}

void apiPair()            // open ONLY in AP setup mode -> hands out the token
{
    if (!setupMode) { sendJson(403, "{\"ok\":false,\"error\":\"pair only in setup mode\"}"); return; }
    JsonDocument doc;
    doc["ok"] = true;
    doc["token"] = deviceToken;
    doc["device_id"] = deviceId;
    String out; serializeJson(doc, out);
    sendJson(200, out);
}

void apiWifiScan()        // open in setup mode, else auth
{
    if (!setupMode && !requireAuth()) return;
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    doc["ok"] = true;
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n && i < 20; i++)
    {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    String out; serializeJson(doc, out);
    sendJson(200, out);
    WiFi.scanDelete();
}

void apiWifiSet()         // open in setup mode, else auth -> saves creds + reboots
{
    if (!setupMode && !requireAuth()) return;
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) { sendJson(400, "{\"ok\":false}"); return; }
    const char *ssid = doc["ssid"] | "";
    const char *pass = doc["pass"] | "";
    if (strlen(ssid) == 0) { sendJson(400, "{\"ok\":false,\"error\":\"ssid required\"}"); return; }
    saveWifi(ssid, pass);
    sendJson(200, "{\"ok\":true,\"msg\":\"saved, rebooting\"}");
    delay(600);
    ESP.restart();
}

void apiStatus()          // auth
{
    if (!requireAuth()) return;
    JsonDocument doc;
    char t[9], d[11];
    if (rtcOk)
    {
        DateTime now = rtc.now();
        medlogic::formatTime(t, now.hour(), now.minute(), now.second());
        medlogic::formatDate(d, now.day(), now.month(), now.year());
    }
    else { strcpy(t, "--:--:--"); strcpy(d, "----------"); }
    doc["ok"] = true;
    doc["time"] = t;
    doc["date"] = d;
    doc["epoch"] = nowEpoch();
    doc["mode"] = setupMode ? "ap" : "sta";
    doc["ap"] = AP_SSID;
    JsonArray arr = doc["chambers"].to<JsonArray>();
    for (int i = 0; i < NUM_CHAMBERS; i++)
    {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = i;
        o["label"] = chambers[i].label;
        o["hour"] = chambers[i].hour;
        o["minute"] = chambers[i].minute;
        o["enabled"] = chambers[i].enabled;
        o["start"] = chambers[i].startDate;
        o["end"] = chambers[i].endDate;
        o["alerting"] = chambers[i].alerting;
        o["taken"] = chambers[i].taken;
        o["taken_count"] = chambers[i].takenCount;
        o["missed_count"] = chambers[i].missedCount;
        o["last_taken"] = epochToStr(chambers[i].lastTakenEpoch);
        o["last_taken_epoch"] = chambers[i].lastTakenEpoch;
    }
    String out; serializeJson(doc, out);
    sendJson(200, out);
}

void apiSchedules()       // auth
{
    if (!requireAuth()) return;
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) { sendJson(400, "{\"ok\":false}"); return; }
    JsonArray arr = doc["chambers"].as<JsonArray>();
    int i = 0;
    for (JsonObject o : arr)
    {
        if (i >= NUM_CHAMBERS) break;
        snprintf(chambers[i].label, sizeof(chambers[i].label), "%s",
                 (const char *)(o["label"] | chambers[i].label));
        chambers[i].hour = constrain((int)(o["hour"] | chambers[i].hour), 0, 23);
        chambers[i].minute = constrain((int)(o["minute"] | chambers[i].minute), 0, 59);
        chambers[i].enabled = o["enabled"] | false;
        if (!o["start"].isNull())
            snprintf(chambers[i].startDate, sizeof(chambers[i].startDate), "%s",
                     (const char *)o["start"]);
        if (!o["end"].isNull())
            snprintf(chambers[i].endDate, sizeof(chambers[i].endDate), "%s",
                     (const char *)o["end"]);
        chambers[i].triggered = false;
        i++;
    }
    saveConfig();
    sendJson(200, "{\"ok\":true}");
}

void apiSetTime()         // auth
{
    if (!requireAuth()) return;
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) || !rtcOk) { sendJson(400, "{\"ok\":false}"); return; }
    if (doc["tz"].is<long>()) { tzOffset = doc["tz"].as<long>(); saveConfig(); }
    if (doc["year"].is<int>())
        rtc.adjust(DateTime((int)doc["year"], (int)doc["month"], (int)doc["day"],
                            (int)doc["hour"], (int)doc["minute"], (int)doc["second"]));
    sendJson(200, "{\"ok\":true}");
}

void apiDismiss()         // auth
{
    if (!requireAuth()) return;
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));
    int i = doc["chamber"] | -1;
    if (i >= 0 && i < NUM_CHAMBERS && chambers[i].alerting)
    {
        chambers[i].alerting = false;
        chambers[i].taken = true;
        chambers[i].takenCount++;
        chambers[i].lastTakenEpoch = nowEpoch();
        digitalWrite(LED_PINS[i], LOW);
        saveConfig();
    }
    sendJson(200, "{\"ok\":true}");
}

void apiTest()            // auth
{
    if (!requireAuth()) return;
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));
    int i = doc["chamber"] | -1;
    if (i >= 0 && i < NUM_CHAMBERS) startAlert(i);
    sendJson(200, "{\"ok\":true}");
}

// HTTP OTA: POST firmware.bin as multipart. Auth checked at upload start.
void apiOtaResult()
{
    bool ok = otaAuthorized && !Update.hasError();
    sendJson(ok ? 200 : (otaAuthorized ? 500 : 401),
             ok ? "{\"ok\":true,\"msg\":\"updated, rebooting\"}"
                : (otaAuthorized ? "{\"ok\":false,\"error\":\"update failed\"}"
                                 : "{\"ok\":false,\"error\":\"unauthorized\"}"));
    if (ok) { delay(600); ESP.restart(); }
}

void apiOtaUpload()
{
    HTTPUpload &up = server.upload();
    if (up.status == UPLOAD_FILE_START)
    {
        otaAuthorized = authed();
        if (!otaAuthorized) { Serial.println("OTA rejected: no token"); return; }
        Serial.printf("OTA start: %s\n", up.filename.c_str());
        Update.begin(UPDATE_SIZE_UNKNOWN);
    }
    else if (up.status == UPLOAD_FILE_WRITE)
    {
        if (otaAuthorized) Update.write(up.buf, up.currentSize);
    }
    else if (up.status == UPLOAD_FILE_END)
    {
        if (otaAuthorized) Update.end(true);
    }
}

void handleRoot() { addCors(); server.send_P(200, "text/html", INDEX_HTML); }

void handleNotFound()
{
    if (server.method() == HTTP_OPTIONS) { addCors(); server.send(204); return; }
    sendJson(404, "{\"ok\":false,\"error\":\"not found\"}");
}

// ===========================================================================
// Network
// ===========================================================================
void startMDNS()
{
    if (MDNS.begin(MDNS_HOST)) MDNS.addService("http", "tcp", 80);
}

void syncNtp()
{
    if (setupMode || !rtcOk) return;
    configTime(tzOffset, 0, "pool.ntp.org", "time.nist.gov");
    struct tm t;
    if (getLocalTime(&t, 6000))
    {
        rtc.adjust(DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                            t.tm_hour, t.tm_min, t.tm_sec));
        Serial.println("RTC synced from NTP");
    }
}

void startNetwork()
{
    loadWifi();
    if (strlen(wifiSsid) > 0)
    {
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSsid, wifiPass);
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(250);
        if (WiFi.status() == WL_CONNECTED)
        {
            setupMode = false;
            ipAddr = WiFi.localIP();
            Serial.printf("STA connected: %s\n", ipAddr.toString().c_str());
            startMDNS();
            syncNtp();
            return;
        }
        Serial.println("STA failed, falling back to AP setup");
    }
    setupMode = true;
    WiFi.mode(WIFI_AP_STA);           // AP for setup; STA enabled so scan works
    WiFi.softAP(AP_SSID, AP_PASS);
    ipAddr = WiFi.softAPIP();
    Serial.printf("AP '%s' http://%s\n", AP_SSID, ipAddr.toString().c_str());
    startMDNS();
}

void startOta()
{
    ArduinoOTA.setHostname(MDNS_HOST);
    ArduinoOTA.setPassword(deviceToken.c_str());
    ArduinoOTA.begin();
}

// ===========================================================================
// Setup / loop
// ===========================================================================
void setup()
{
    Serial.begin(115200);
    bootMillis = millis();
    Wire.begin(SDA_PIN, SCL_PIN);

    lcd.init();
    lcd.backlight();
    lcd.print("SmartMed boot...");

    rtcOk = rtc.begin();
    if (!rtcOk) Serial.println("RTC NOT FOUND");

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    for (int i = 0; i < NUM_CHAMBERS; i++)
    {
        pinMode(REED_PINS[i], INPUT_PULLUP);
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }

    if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
    loadConfig();
    loadOrCreateToken();

    uint64_t mac = ESP.getEfuseMac();
    char idbuf[13];
    snprintf(idbuf, sizeof(idbuf), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
    deviceId = String(idbuf);

    startNetwork();
    startOta();

    const char *hdrs[] = {"Authorization"};
    server.collectHeaders(hdrs, 1);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/v1/health",   HTTP_GET,  apiHealth);
    server.on("/api/v1/info",     HTTP_GET,  apiInfo);
    server.on("/api/v1/pair",     HTTP_GET,  apiPair);
    server.on("/api/v1/wifi/scan",HTTP_GET,  apiWifiScan);
    server.on("/api/v1/wifi",     HTTP_POST, apiWifiSet);
    server.on("/api/v1/status",   HTTP_GET,  apiStatus);
    server.on("/api/v1/schedules",HTTP_POST, apiSchedules);
    server.on("/api/v1/time",     HTTP_POST, apiSetTime);
    server.on("/api/v1/dismiss",  HTTP_POST, apiDismiss);
    server.on("/api/v1/test",     HTTP_POST, apiTest);
    server.on("/api/v1/ota",      HTTP_POST, apiOtaResult, apiOtaUpload);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.printf("Device %s  token %s\n", deviceId.c_str(), deviceToken.c_str());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(setupMode ? "Setup: connect" : "Connected");
    lcd.setCursor(0, 1);
    lcd.print(setupMode ? AP_SSID : ipAddr.toString());
    delay(2500);
    lcd.clear();
}

void loop()
{
    server.handleClient();
    ArduinoOTA.handle();

    static uint32_t lastTick = 0;
    if (millis() - lastTick >= 200)
    {
        lastTick = millis();
        checkSchedule();
        int active = handleAlerts();
        updateLcd(active);
    }
}
