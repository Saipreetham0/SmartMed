#ifndef MEDLOGIC_H
#define MEDLOGIC_H

#include <stdint.h>

// Pure, hardware-free logic for the SmartMed medicine reminder.
// Nothing in here touches the RTC, LCD, buzzer or pins, so it can be
// compiled and unit-tested natively (on the host) without an ESP32.
namespace medlogic
{
    // One dose time.
    struct MedicineSchedule
    {
        uint8_t hour;
        uint8_t minute;
    };

    static const int BOX_COUNT = 6;
    static const int REED_OPEN = 1;   // digitalRead HIGH == box opened (INPUT_PULLUP)
    static const int REED_CLOSED = 0; // digitalRead LOW  == box closed

    // Should a reminder fire right now for this dose?
    // True only when the current time matches AND it has not already fired.
    bool shouldTrigger(const MedicineSchedule &s,
                       bool alreadyTriggered,
                       int nowHour,
                       int nowMinute);

    // Should the "already fired" flag be cleared?
    // We reset once the clock has left the scheduled hour, so the dose can
    // arm again for the next day.
    bool shouldReset(const MedicineSchedule &s, int nowHour);

    // Is the box reported open by its reed switch reading?
    // openLevel is the digitalRead value that means "open" for this sensor:
    // REED_OPEN for a normal (HIGH-active) box, REED_CLOSED for a reversed one.
    bool isBoxOpen(int reedReading, int openLevel = REED_OPEN);

    // Format "HH:MM:SS" into buf (needs at least 9 bytes). Returns buf.
    char *formatTime(char *buf, int hour, int minute, int second);

    // Format "DD/MM/YYYY" into buf (needs at least 11 bytes). Returns buf.
    char *formatDate(char *buf, int day, int month, int year);
}

#endif // MEDLOGIC_H
