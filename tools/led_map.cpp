// ---------------------------------------------------------------------------
// LED mapper - not part of the firmware.
// Lights ONE LED at a time (by array index) and shows that index on the LCD,
// so you can see which physical box each LED lives in.
//
// Upload:  pio run -e led_map -t upload
// Watch the boxes: note which physical box lights at each "LED idx: N".
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SDA_PIN 21
#define SCL_PIN 22

const uint8_t ledPins[6] = {32, 33, 16, 17, 4, 13};

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.init();
    lcd.backlight();
    for (int i = 0; i < 6; i++)
    {
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], LOW);
    }
}

void loop()
{
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 6; j++) digitalWrite(ledPins[j], LOW);
        digitalWrite(ledPins[i], HIGH);

        char msg[17];
        sprintf(msg, "LED idx: %d  G%-2d ", i + 1, ledPins[i]);
        lcd.setCursor(0, 0);
        lcd.print(msg);
        lcd.setCursor(0, 1);
        lcd.print("Which box lights?");

        Serial.print("LED idx=");
        Serial.print(i + 1);
        Serial.print("  GPIO=");
        Serial.println(ledPins[i]);

        delay(5000);   // hold each LED 5s so you can note the physical box
    }
}
