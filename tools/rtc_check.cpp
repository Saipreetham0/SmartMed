// ---------------------------------------------------------------------------
// RTC (DS3231) live check + one-time set - not part of the firmware.
// Shows the current time/date on the LCD and Serial, and can set the clock
// when it receives a line:  SET,YYYY,MM,DD,HH,MM,SS
//
// Upload:  pio run -e rtc_check -t upload
// Monitor: pio device monitor -b 115200
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include "MedLogic.h"

#define SDA_PIN 21
#define SCL_PIN 22

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);

    lcd.init();
    lcd.backlight();

    if (!rtc.begin())
    {
        Serial.println("RTC NOT FOUND");
        lcd.setCursor(0, 0);
        lcd.print("RTC NOT FOUND");
        while (1) delay(100);
    }

    if (rtc.lostPower())
    {
        Serial.println("RTC lost power - time may be wrong!");
        lcd.setCursor(0, 0);
        lcd.print("RTC lost power!");
        delay(2000);
    }
}

// Accept "SET,YYYY,MM,DD,HH,MM,SS" over serial to set the clock.
void handleSerialSet()
{
    static String buf;
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\n')
        {
            if (buf.startsWith("SET,"))
            {
                int y, mo, d, h, mi, s;
                if (sscanf(buf.c_str(), "SET,%d,%d,%d,%d,%d,%d",
                           &y, &mo, &d, &h, &mi, &s) == 6)
                {
                    rtc.adjust(DateTime(y, mo, d, h, mi, s));
                    Serial.println("RTC SET OK");
                }
            }
            buf = "";
        }
        else if (c != '\r')
        {
            buf += c;
        }
    }
}

void loop()
{
    handleSerialSet();

    DateTime now = rtc.now();

    char line1[17];
    char line2[17];
    medlogic::formatTime(line1, now.hour(), now.minute(), now.second());
    medlogic::formatDate(line2, now.day(), now.month(), now.year());

    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print(line2);
    lcd.print("      ");

    Serial.print(line2);
    Serial.print("  ");
    Serial.print(line1);
    Serial.print("   Temp: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");

    delay(1000);
}
