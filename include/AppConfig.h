#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <Arduino.h>

// Firmware version (reported by /api/v1/info; bump on releases).
#define FW_VERSION "2.0.0"

// Number of medicine chambers.
#define NUM_CHAMBERS 6

// mDNS host -> device reachable at http://smartmed.local
#define MDNS_HOST "smartmed"

// Default timezone offset from UTC in seconds (IST = +5:30). Editable via API.
#define DEFAULT_TZ_OFFSET 19800

// I2C (RTC + LCD)
#define SDA_PIN 21
#define SCL_PIN 22

// Active buzzer
#define BUZZER_PIN 14

// WiFi Access Point - connect a phone/laptop to this network, then browse
// to http://192.168.4.1
static const char *AP_SSID = "Medicine Reminder";
static const char *AP_PASS = "medkit123";      // >= 8 chars for WPA2

// --- Hardware maps (physical box 1..6 order, from the bench mapping) --------
static const uint8_t REED_PINS[NUM_CHAMBERS] = {19, 25, 26, 23, 18, 27};
static const uint8_t LED_PINS[NUM_CHAMBERS]  = {16, 4, 32, 17, 13, 33};

// digitalRead level that means "box open" per chamber.
// Box 3's MC-38 is reversed (LOW-active); all others are HIGH-active.
static const uint8_t REED_OPEN_LEVEL[NUM_CHAMBERS] = {HIGH, HIGH, LOW, HIGH, HIGH, HIGH};

// A missed dose is recorded if the box is not opened within this many
// seconds of the reminder starting; the alert then auto-clears.
static const uint32_t ALERT_TIMEOUT_SECONDS = 3600; // 1 hour

// One medicine chamber: schedule + adherence + live alert state.
struct Chamber
{
    // persisted
    char     label[24];
    uint8_t  hour;
    uint8_t  minute;
    bool     enabled;
    char     startDate[11];  // "YYYY-MM-DD", empty = no start bound
    char     endDate[11];    // "YYYY-MM-DD", empty = no end bound
    uint32_t takenCount;
    uint32_t missedCount;
    uint32_t lastTakenEpoch;

    // runtime only (not saved)
    bool     triggered;      // fired for the current matching minute
    bool     alerting;       // currently reminding
    bool     taken;          // taken since the last trigger
    uint32_t triggerEpoch;   // when the current alert started
};

#endif // APPCONFIG_H
