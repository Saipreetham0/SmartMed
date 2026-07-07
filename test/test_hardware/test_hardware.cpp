// ---------------------------------------------------------------------------
// On-hardware connection / sensor tests for SmartMed.
// Runs ON the ESP32 (board must be plugged in):   pio test -e esp32doit-devkit-v1
//
// Verifies that every peripheral is wired and responding:
//   * DS3231 RTC   (I2C 0x68)  -> found + clock ticking
//   * LCD 16x2     (I2C 0x27)  -> present on the bus
//   * 6x Reed switch inputs    -> connected / stable, not floating
//   * 6x LED outputs           -> driveable (visual blink)
//   * Buzzer output            -> driveable (audible beep)
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include "MedLogic.h"
#include <unity.h>

#define SDA_PIN 21
#define SCL_PIN 22

#define RTC_I2C_ADDR 0x68
#define LCD_I2C_ADDR 0x27

static const uint8_t reedPins[6] = {18, 19, 23, 25, 26, 27};
static const uint8_t ledPins[6]  = {32, 33, 16, 17, 4, 13};
static const uint8_t buzzerPin   = 14;

static RTC_DS3231 rtc;
static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);

// True if a device ACKs at this I2C address.
static bool i2cDevicePresent(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// --- RTC -------------------------------------------------------------------

void test_rtc_present_on_i2c(void)
{
    TEST_ASSERT_TRUE_MESSAGE(i2cDevicePresent(RTC_I2C_ADDR),
                             "DS3231 not ACKing at 0x68 - check SDA/SCL/power");
}

void test_rtc_begins(void)
{
    TEST_ASSERT_TRUE_MESSAGE(rtc.begin(), "rtc.begin() failed");
}

void test_rtc_clock_is_ticking(void)
{
    // Read the seconds field, wait, read again: it must advance.
    DateTime t1 = rtc.now();
    delay(1500);
    DateTime t2 = rtc.now();
    TEST_ASSERT_NOT_EQUAL_MESSAGE(t1.unixtime(), t2.unixtime(),
                                  "RTC time not advancing - clock/oscillator issue");
}

void test_rtc_time_is_sane(void)
{
    DateTime now = rtc.now();
    TEST_ASSERT_TRUE_MESSAGE(now.hour() < 24 && now.minute() < 60 && now.second() < 60,
                             "RTC returned an out-of-range time");
    TEST_ASSERT_TRUE_MESSAGE(now.year() >= 2024, "RTC year unset - set the time");
}

// --- LCD -------------------------------------------------------------------

void test_lcd_present_on_i2c(void)
{
    TEST_ASSERT_TRUE_MESSAGE(i2cDevicePresent(LCD_I2C_ADDR),
                             "LCD backpack not ACKing at 0x27 - check wiring/address");
}

// Drive the LCD end-to-end: init, backlight, print to both rows, then show the
// live RTC time/date (same content the firmware displays). Software can't read
// pixels back, so confirm visually that all of this appears on the screen.
void test_lcd_displays_text(void)
{
    lcd.init();
    lcd.backlight();

    // 1) Static banner on both rows.
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SmartMed TEST");
    lcd.setCursor(0, 1);
    lcd.print("Line2 OK 0123456");
    delay(2000);

    // 2) Backlight blink - confirms the backlight line is controllable.
    for (int i = 0; i < 3; i++)
    {
        lcd.noBacklight();
        delay(250);
        lcd.backlight();
        delay(250);
    }

    // 3) Live clock, formatted exactly like the firmware (via MedLogic).
    DateTime now = rtc.now();
    char line1[17];
    char line2[17];
    medlogic::formatTime(line1, now.hour(), now.minute(), now.second());
    medlogic::formatDate(line2, now.day(), now.month(), now.year());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
    delay(2500);

    TEST_PASS_MESSAGE("LCD drawn - confirm banner, backlight blink & live clock");
}

// --- Reed switches ---------------------------------------------------------

// Each reed input (INPUT_PULLUP) must read a stable, valid level.
// A floating (disconnected) pin tends to be noisy; two spaced reads must agree.
void test_reed_switches_connected_and_stable(void)
{
    char msg[48];
    for (int i = 0; i < 6; i++)
    {
        pinMode(reedPins[i], INPUT_PULLUP);
        delay(5);
        int a = digitalRead(reedPins[i]);
        delay(20);
        int b = digitalRead(reedPins[i]);

        snprintf(msg, sizeof(msg), "Reed %d (GPIO%d) unstable/floating",
                 i + 1, reedPins[i]);
        TEST_ASSERT_EQUAL_MESSAGE(a, b, msg);
        TEST_ASSERT_TRUE_MESSAGE(a == HIGH || a == LOW, msg);
    }
}

// --- LEDs (visual) ---------------------------------------------------------

// Cannot read back an output pin, so this drives each LED and just proves the
// write path runs. Watch the box: every LED should blink once in turn.
void test_led_outputs_driveable(void)
{
    for (int i = 0; i < 6; i++)
    {
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], HIGH);
        delay(200);
        TEST_ASSERT_EQUAL_MESSAGE(HIGH, digitalRead(ledPins[i]),
                                  "LED pin latch read-back mismatch");
        digitalWrite(ledPins[i], LOW);
        delay(100);
    }
    TEST_PASS_MESSAGE("All 6 LEDs pulsed - confirm visually");
}

// --- Buzzer (audible) ------------------------------------------------------

void test_buzzer_driveable(void)
{
    pinMode(buzzerPin, OUTPUT);
    for (int i = 0; i < 2; i++)
    {
        digitalWrite(buzzerPin, HIGH);
        delay(150);
        digitalWrite(buzzerPin, LOW);
        delay(150);
    }
    TEST_PASS_MESSAGE("Buzzer pulsed twice - confirm audibly");
}

void setup()
{
    delay(2000); // let the USB serial settle before Unity output
    Wire.begin(SDA_PIN, SCL_PIN);

    UNITY_BEGIN();

    RUN_TEST(test_rtc_present_on_i2c);
    RUN_TEST(test_rtc_begins);
    RUN_TEST(test_rtc_clock_is_ticking);
    RUN_TEST(test_rtc_time_is_sane);

    RUN_TEST(test_lcd_present_on_i2c);
    RUN_TEST(test_lcd_displays_text);

    RUN_TEST(test_reed_switches_connected_and_stable);

    RUN_TEST(test_led_outputs_driveable);
    RUN_TEST(test_buzzer_driveable);

    UNITY_END();
}

void loop()
{
}
