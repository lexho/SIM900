#include <sim900.h>

bool led_state = LOW;
unsigned long start_time_buffer = 0;
unsigned long rtcSyncTime = 0; // For rollover-safe daily tasks

const byte serialCmdBufferSize = 255;
char serialCmdBuffer[serialCmdBufferSize];
byte serialCmdBufferIdx = 0;

SIM900 sim900(Serial1);

class OnOKListener : public EventListener {
public:
  OnOKListener() { type = SIM900_OK; }
  bool execute() override { Serial.println(F("received OK.")); }
};

// Define the OnCallListener class at the global scope
class OnCallListener : public EventListener {
  unsigned long lastRingMessageTime = 0; // Timestamp for the last "ringing" message
public:
    OnCallListener() {
      type = SIM900_RING; // Set the type inherited from EventListener
    }

    bool execute() override {
        // This code will now be executed from within sim900.handleEvents()
        if (!sim900.isCalling()) { // On the very first ring, set the state and start time.
          sim900.calling = true;
          lastRingMessageTime = millis(); // Set initial time for the first message
          Serial.println(F("someone is calling..."));
        }
        
        if(sim900.isCalling() && (millis() - lastRingMessageTime > 1000)) {
          Serial.print(sim900.getPhoneNumber()); Serial.println(F(" is calling..."));
          lastRingMessageTime = millis();
        }
        return true;
    }
};

class OnNOCARRIERListener : public EventListener {
  char buffer[161];  // Max SMS length + null terminator
  char buffer_old[161];
public:
  OnNOCARRIERListener() {
    type = SIM900_NOCARRIER;
    buffer[0] = '\0';      // Initialize buffer to an empty string
    buffer_old[0] = '\0';  // Initialize buffer_old 
  }

  uint8_t day;
  uint8_t month;
  uint8_t year;
  const unsigned long oneDay = 1000UL * 60 * 60 * 24;
  const unsigned long oneMinute = 60UL * 1000; // 60,000 milliseconds
  unsigned long message_age = 0; // in milliseconds since midnight
  unsigned long rtcSyncMillis = 0; // millis() value at the last RTC sync
  unsigned long rtcMilliSeconds = 0; // Time in seconds since midnight at the last RTC sync

  // Store the time from RTC and the millis() value at the time of sync
  void storeRTC(SIM900RTC rtc) {
    day = rtc.day;
    month = rtc.month;
    year = rtc.year;
    rtcSyncMillis = millis();
    rtcMilliSeconds = ((unsigned long)rtc.hour * 3600UL + (unsigned long)rtc.minute * 60UL + (unsigned long)rtc.second) * 1000UL;
  }

  // Calculate elapsed seconds since last sync, safe from millis() rollover
  unsigned long getMillisSinceMidnight() {
    unsigned long elapsedMillis = millis() - rtcSyncMillis;
    return (rtcMilliSeconds + elapsedMillis) % oneDay;
  }

  bool sendBufferedMessageViaSMS() {
    if((message_age + oneMinute*4) > getMillisSinceMidnight()) { // is dataset older than 4 minutes?
      return sim900.sendSMSRoutine(buffer);
    } else {
      return sim900.sendSMSRoutine("Keine aktuellen Wetterdaten verfugbar.");
    }
  }

  void copyToBuffer(const char* message) {
    strncpy(buffer, message, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';  // GUARANTEE null termination
  }

  int sizeOfBuffer() {
    return sizeof(buffer);
  }

  void printBuffer() {
    if (strcmp(buffer, buffer_old) != 0 && strlen(buffer) > 0) {
      Serial.print(F("buffer: \""));
      Serial.print(buffer);
      Serial.println("\"");
      strcpy(buffer_old, buffer);
    }
  }

  bool execute() override {
    if(sim900.isCalling()) {
      sim900.calling = false;
      Serial.println(F("call ended. preparing to send sms..."));
      sendBufferedMessageViaSMS();
    }
  }
};

OnNOCARRIERListener* onNocarrierPtr = nullptr;

class OnCMTListener : public EventListener {
public:
  OnCMTListener() { type = SIM900_CMT; }
  bool execute() override { 
    // Clear the serial buffer to discard the SMS message body, as we don't need it.
    //while(Serial1.available()) { Serial1.read(); }
    sim900.clearBuffer();
    Serial.print(F("received an sms from "));
    Serial.println(sim900.getPhoneNumber());
    onNocarrierPtr->sendBufferedMessageViaSMS();
  }
};

void setup() {
  Serial.begin(19200);
  Serial1.begin(9600);
  while(!Serial);
  while(!Serial1);
  while(!sim900.bootstrap()) {
    delay(5000); // Wait before retrying
  }

  // Create and register the listener so it's ready for events.
  auto onOK = std::make_unique<OnOKListener>();
  auto onCall = std::make_unique<OnCallListener>();
  auto onNocarrier = std::make_unique<OnNOCARRIERListener>();
  auto onCmt = std::make_unique<OnCMTListener>();
  onNocarrierPtr = onNocarrier.get();
  sim900.registerListener(std::move(onOK));
  sim900.registerListener(std::move(onCall)); // register a on call listener which prints " is calling" to Serial
  sim900.registerListener(std::move(onNocarrier));
  sim900.registerListener(std::move(onCmt));

  // Get time from rtc and store it in the listener *after* it has been created.
  SIM900RTC current = sim900.rtc();
  onNocarrierPtr->storeRTC(current);
  printRTC(current);

  Serial.println(F("waiting for incoming data..."));
}

void loop() {
  digitalWrite(STATUS_LED, led_state); led_state = !led_state;

  // Every 2 seconds print the message in the buffer
  if (millis() - start_time_buffer >= 2000) {
    start_time_buffer = millis();
    onNocarrierPtr->printBuffer();
  }

  // Update RTC once per day
  if (millis() - rtcSyncTime >= onNocarrierPtr->oneDay) {
    rtcSyncTime = millis();
    SIM900RTC current = sim900.rtc();
    onNocarrierPtr->storeRTC(current);
  }

  /*while(Serial1.available()) {
    Serial.write(Serial1.read());
  }*/

  SIM900_Handler_Event event = sim900.handleEvents();

  // Non-blocking serial command reader
  while (Serial.available() > 0) {
    char receivedChar = Serial.read();

    if (receivedChar == '\n' || receivedChar == '\r') {
      if (serialCmdBufferIdx > 0) { // If we have a command
        serialCmdBuffer[serialCmdBufferIdx] = '\0'; // Null-terminate the string
        parseCommand(serialCmdBuffer); // size 255
        serialCmdBufferIdx = 0; // Reset for next command
      }
    } else {
      if (serialCmdBufferIdx < serialCmdBufferSize - 1) {
        serialCmdBuffer[serialCmdBufferIdx++] = receivedChar;
      }
    }
  }

  delay(50); // Reduced delay to make loop more responsive
}

void parseCommand(const char* command) {
  const char* storebuffer_cmd = "storebuffer";
  if (strncmp(command, storebuffer_cmd, strlen(storebuffer_cmd)) == 0) {
    const char* message = command + strlen(storebuffer_cmd);

    // Check if there is data after "storebuffer". It should start with a space.
    if (*message != '\0' && *message != ' ') {
        // The command is something like "storebufferXYZ", which is invalid.
        return; 
    }
    if (*message == ' ') { // Skip the leading space if it exists.
        message++;
    }

    if (strlen(message) > 0) { // We have data to parse
      // Ensure the message isn't too long for our buffer
      if (strlen(message) >= onNocarrierPtr->sizeOfBuffer()) {
        Serial.print(F("error: message is too long\n"));
        return;
      }
      // Find "time: HH:MM:SS" and parse it
      const char* time_ptr = strstr(message, "time: ");
      if (time_ptr != NULL) {
        time_ptr += 6; // Move pointer past "time: " to the start of HH:MM:SS
        unsigned long total_seconds = ((time_ptr[0] - '0') * 10UL + (time_ptr[1] - '0')) * 3600UL +
                                      ((time_ptr[3] - '0') * 10UL + (time_ptr[4] - '0')) * 60UL +
                                      ((time_ptr[6] - '0') * 10UL + (time_ptr[7] - '0'));
        onNocarrierPtr->message_age = total_seconds * 1000UL;
      }
      // Safely copy the message to the global buffer
      onNocarrierPtr->copyToBuffer(message);
    } else {
      onNocarrierPtr->printBuffer();
    }

  } else if (strcmp(command, "time") == 0) {
    char timeBuffer[25];
    getFakeHardwareClockTime(timeBuffer, sizeof(timeBuffer));
    while(millis() % 1000 != 0); // print time at precise time intervals
    Serial.print("time: "); Serial.println(timeBuffer);
  } else if (strncmp(command, "sendsms", 7) == 0) {
    // C-string implementation to parse "sendsms [phonenumber] [message]"
    const char* phone_start = strchr(command, ' ');
    if (phone_start == nullptr) {
      Serial.println(F("error: malformed sendsms command. usage: sendsms <number> <message>"));
      return;
    }
    phone_start++; // Move past the first space

    const char* message_start = strchr(phone_start, ' ');
    if (message_start == nullptr) {
      Serial.println(F("error: malformed sendsms command. missing message."));
      return;
    }

    // Extract the phone number into a separate buffer
    char phonenumber[20]; // Buffer for the phone number
    size_t phone_len = message_start - phone_start;
    if (phone_len > 0 && phone_len < sizeof(phonenumber)) {
      strncpy(phonenumber, phone_start, phone_len);
      phonenumber[phone_len] = '\0'; // Null-terminate the phone number string

      message_start++; // Move past the space to the actual message
      sim900.sendSMSRoutine(phonenumber, message_start);
    } else {
      Serial.println(F("error: invalid phone number."));
    }
  }  //else
}

// hour:minute:seconds
void getFakeHardwareClockTime(char* buffer, size_t bufferSize) {
  // This function still works with seconds for display purposes
  unsigned long now_seconds = onNocarrierPtr->getMillisSinceMidnight() / 1000UL;
  
  int s = now_seconds % 60;
  unsigned long total_minutes = now_seconds / 60;
  int min = total_minutes % 60;
  unsigned long total_hours = total_minutes / 60;
  int h = total_hours % 24;

  snprintf(buffer, bufferSize,
           "%02u.%02u.%02u %02u:%02u:%02u",
           onNocarrierPtr->day, onNocarrierPtr->month, onNocarrierPtr->year,
           h, min, s);
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