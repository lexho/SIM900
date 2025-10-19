#include <Arduino.h>
#include "time.h"

const unsigned long Time::oneMinute = 60UL * 1000; // 60,000 milliseconds
const unsigned long Time::oneHour = oneMinute * 60;
const unsigned long Time::oneDay = oneHour * 24;

// Initialize static member variables
uint8_t Time::day = 0;
uint8_t Time::month = 0;
uint8_t Time::year = 0;
unsigned long Time::rtcSyncMillis = 0;
unsigned long Time::rtcMilliSeconds = 0; // Time in milliseconds since midnight

// Store the time from RTC and the millis() value at the time of sync
void Time::storeRTC(SIM900RTC rtc)
{
    Time::day = rtc.day;
    Time::month = rtc.month;
    Time::year = rtc.year;
    Time::rtcSyncMillis = millis(); // last rtc sync in milliseconds since program start
    Time::rtcMilliSeconds = ((unsigned long)rtc.hour * 3600UL + (unsigned long)rtc.minute * 60UL + (unsigned long)rtc.second) * 1000UL;
}

unsigned long Time::getMillisSinceMidnight() {
    unsigned long elapsedMillis = millis() - Time::rtcSyncMillis;
    return (Time::rtcMilliSeconds + elapsedMillis) % Time::oneDay;
}

// hour:minute:seconds
void Time::getFakeHardwareClockTime(char* buffer, size_t bufferSize) {
  // This function still works with seconds for display purposes
  unsigned long now_seconds = Time::getMillisSinceMidnight() / 1000UL;
  
  int s = now_seconds % 60;
  unsigned long total_minutes = now_seconds / 60;
  int min = total_minutes % 60;
  unsigned long total_hours = total_minutes / 60;
  int h = total_hours % 24;

  snprintf(buffer, bufferSize,
           "%02u.%02u.%02u %02u:%02u:%02u",
           Time::day, Time::month, Time::year,
           h, min, s);
}

void Time::printRTC(SIM900RTC datetime) {
  int day = datetime.day;
  int month = datetime.month;
  int year = datetime.year;

  int h = datetime.hour;
  int m = datetime.minute;
  int s = datetime.second;

  Serial.print("rtc: ");
  Serial.print(day>9 ? "" : "0"); Serial.print(day); Serial.print(".");
  Serial.print(month>9 ? "" : "0"); Serial.print(month); Serial.print(".");
  Serial.print(year>9 ? "" : "0"); Serial.print(year); Serial.print(" ");

  Serial.print(h>9 ? "" : "0"); Serial.print(h); Serial.print(":");
  Serial.print(m>9 ? "" : "0"); Serial.print(m); Serial.print(":");
  Serial.print(s>9 ? "" : "0"); Serial.println(s);
}