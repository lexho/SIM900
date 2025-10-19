#ifndef TIME_H
#define TIME_H

#include <cstdint>
#include "sim900.h"

class Time {
public:
    static const unsigned long oneDay;
    static const unsigned long oneHour;
    static const unsigned long oneMinute;

    static uint8_t day;
    static uint8_t month;
    static uint8_t year;

    static unsigned long rtcSyncMillis; // millis() value at the last RTC sync
    static unsigned long rtcMilliSeconds; // Time in seconds since midnight at the last RTC sync

    static void storeRTC(SIM900RTC rtc);
    static unsigned long getMillisSinceMidnight();
    static void getFakeHardwareClockTime(char* buffer, size_t bufferSize);

    static void printRTC(SIM900RTC datetime);
};

#endif // TIMECONSTANTS_H