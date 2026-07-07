// ---------------------------------------------------------------------------
// Reed-switch / LED mapper - not part of the firmware.
// Lights the LED for whichever reed you OPEN and logs it, so you can discover
// which physical box (1..6) each GPIO belongs to.
//
// Upload:  pio run -e reed_map -t upload
// Then open each box IN PHYSICAL ORDER (1,2,3,...) and watch the serial log /
// LCD to see which array index + GPIO reacts.
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SDA_PIN 21
#define SCL_PIN 22

// Same arrays as the firmware - index i == "box i+1" in code today.
const uint8_t reedPins[6] = {18, 19, 23, 25, 26, 27};
const uint8_t ledPins[6]  = {32, 33, 16, 17, 4, 13};

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool prevOpen[6] = {false, false, false, false, false, false};

void setup()
{
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Open a box...");

    for (int i = 0; i < 6; i++)
    {
        pinMode(reedPins[i], INPUT_PULLUP);
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], LOW);
    }

    Serial.println("Reed mapper ready. Open boxes one at a time.");
}

void loop()
{
    char row[17];        // live map on LCD row 2: index1..6 -> O(open)/-(closed)
    char snap[64];       // raw state line for serial
    int p = 0;
    p += sprintf(snap + p, "STATE ");

    for (int i = 0; i < 6; i++)
    {
        int raw = digitalRead(reedPins[i]);
        bool open = (raw == HIGH);
        digitalWrite(ledPins[i], open ? HIGH : LOW);   // its own LED follows it
        row[i] = open ? 'O' : '-';

        // idx | GPIO | H(open)/L(closed)
        p += sprintf(snap + p, "%d:G%d=%c ",
                     i + 1, reedPins[i], open ? 'H' : 'L');

        if (open && !prevOpen[i])
        {
            Serial.print("OPENED  idx=");
            Serial.print(i + 1);
            Serial.print("  reedGPIO=");
            Serial.print(reedPins[i]);
            Serial.print("  ledGPIO=");
            Serial.println(ledPins[i]);
        }
        if (!open && prevOpen[i])
        {
            Serial.print("CLOSED  idx=");
            Serial.print(i + 1);
            Serial.print("  reedGPIO=");
            Serial.println(reedPins[i]);
        }
        prevOpen[i] = open;
    }
    row[6] = '\0';

    // Top line: plainly name whichever index is currently CLOSED (magnet on).
    // 'O' means open/H, so a '-' at position i means box i is closed.
    int closedIdx = -1;
    for (int i = 0; i < 6; i++)
    {
        if (row[i] == '-') { closedIdx = i + 1; break; }
    }
    lcd.setCursor(0, 0);
    if (closedIdx > 0)
    {
        char msg[17];
        sprintf(msg, "Closed idx: %d    ", closedIdx);
        lcd.print(msg);
    }
    else
    {
        lcd.print("Close a box...  ");
    }

    lcd.setCursor(0, 1);
    lcd.print("123456:");
    lcd.print(row);

    Serial.println(snap);   // continuous raw snapshot every cycle
    delay(300);
}
