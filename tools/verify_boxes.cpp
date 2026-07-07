// ---------------------------------------------------------------------------
// Box alignment verifier - not part of the firmware.
// Uses the SAME physical-order arrays as the firmware. Each box's LED follows
// its own reed: OPEN a box and ITS light turns on. If reed & LED are aligned,
// the box you open is the box that lights.
//
// Upload:  pio run -e verify_boxes -t upload
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SDA_PIN 21
#define SCL_PIN 22

// Must match src/main.cpp exactly.
const uint8_t reedPins[6] = {19, 25, 26, 23, 18, 27};
const uint8_t ledPins[6]  = {16, 4, 32, 17, 13, 33};
// Box 3's MC-38 is reversed (LOW-active); all others HIGH-active.
const uint8_t reedOpenLevel[6] = {HIGH, HIGH, LOW, HIGH, HIGH, HIGH};

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.init();
    lcd.backlight();
    for (int i = 0; i < 6; i++)
    {
        pinMode(reedPins[i], INPUT_PULLUP);
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], LOW);
    }
    lcd.setCursor(0, 0);
    lcd.print("Open any box ->");
    lcd.setCursor(0, 1);
    lcd.print("its LED turns on");
    delay(1500);
}

void loop()
{
    char row[17];
    int openBox = 0;                 // first open box (for the LCD headline)

    for (int i = 0; i < 6; i++)
    {
        bool open = (digitalRead(reedPins[i]) == reedOpenLevel[i]);
        digitalWrite(ledPins[i], open ? HIGH : LOW);   // box i lights when open
        row[i] = open ? 'O' : '-';
        if (open && openBox == 0) openBox = i + 1;
    }
    row[6] = '\0';

    lcd.setCursor(0, 0);
    if (openBox > 0)
    {
        char msg[17];
        snprintf(msg, sizeof(msg), "Box %d OPEN lit  ", openBox);
        lcd.print(msg);
    }
    else
    {
        lcd.print("All closed      ");
    }
    lcd.setCursor(0, 1);
    lcd.print("open123456:");
    lcd.print(row);

    delay(100);
}
