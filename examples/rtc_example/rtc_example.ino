#include <sim900.h>

SIM900 sim900(Serial1);
bool led_state = LOW;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  while(!Serial);
  while(!Serial1);

  SIM900RTC current = sim900.rtc();
  printRTC(current);
}

void loop() { 
  digitalWrite(STATUS_LED, led_state); led_state = !led_state;
  delay(100);
}

void printRTC(SIM900RTC datetime) {
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