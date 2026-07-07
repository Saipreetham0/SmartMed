#include "MedLogic.h"
#include <stdio.h>

namespace medlogic
{
    bool shouldTrigger(const MedicineSchedule &s,
                       bool alreadyTriggered,
                       int nowHour,
                       int nowMinute)
    {
        return nowHour == s.hour &&
               nowMinute == s.minute &&
               !alreadyTriggered;
    }

    bool shouldReset(const MedicineSchedule &s, int nowHour)
    {
        return nowHour != s.hour;
    }

    bool isBoxOpen(int reedReading, int openLevel)
    {
        return reedReading == openLevel;
    }

    char *formatTime(char *buf, int hour, int minute, int second)
    {
        sprintf(buf, "%02d:%02d:%02d", hour, minute, second);
        return buf;
    }

    char *formatDate(char *buf, int day, int month, int year)
    {
        sprintf(buf, "%02d/%02d/%04d", day, month, year);
        return buf;
    }
}
