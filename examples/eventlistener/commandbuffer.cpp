#include <Arduino.h> // For Serial, millis(), F()
#include <cstring>   // For strcmp, strlen, strncmp, strstr, strchr, strncpy
#include "commandbuffer.h"
#include "time.h"    // For Time::

const char* CommandBuffer::storebuffer_cmd = "storebuffer";
char CommandBuffer::phonenumber_buffer[20]; // Definition for the static buffer

Message CommandBuffer::read(char receivedChar) {
    if (receivedChar == '\n' || receivedChar == '\r') {
      if (serialCmdBufferIdx > 0) { // If we have a command
        serialCmdBuffer[serialCmdBufferIdx] = '\0'; // Null-terminate the string
        Message msg = parseCommand(serialCmdBuffer);
        serialCmdBufferIdx = 0; // Reset for next command
        return msg;
      } else { // An empty command was entered (e.g., just pressing Enter)
        serialCmdBufferIdx = 0; // Ensure buffer is reset
        return {EMPTY, nullptr, 0, nullptr};
      }
    } else {
      if (serialCmdBufferIdx < serialCmdBufferSize - 1) {
        serialCmdBuffer[serialCmdBufferIdx++] = receivedChar;
      }
    }
    return {INVALID, nullptr, 0, nullptr}; // Return an invalid message if no command is ready
}

Message CommandBuffer::parseCommand(const char* command) {
  if (strncmp(command, CommandBuffer::storebuffer_cmd, strlen(CommandBuffer::storebuffer_cmd)) == 0) {
     return parseStorebuffer(command);
    } else if (strcmp(command, "time") == 0) { return parseTime(); }
      else if (strncmp(command, "sendsms", 7) == 0) { return parseSendsms(command); }
  // If no command matches, return an invalid message
  return {INVALID, nullptr, 0, nullptr};
}

Message CommandBuffer::parseStorebuffer(const char* command) {
    const char* message = command + strlen(CommandBuffer::storebuffer_cmd);
    unsigned long message_received_time = 0; // Initialize to zero

    // Check if there is data after "storebuffer". It should start with a space.
    if (*message != '\0' && *message != ' ') {
        // The command is something like "storebufferXYZ", which is invalid.
        return {STOREBUFFER, nullptr, 0, nullptr}; 
    }
    if (*message == ' ') { // Skip the leading space if it exists.
        message++;
    }

    const char* time_ptr = strstr(message, "time: ");
    if (time_ptr != nullptr) {
        // Basic validation to prevent reading past the end of the string
        if (strlen(time_ptr) >= 14) { // "time: HH:MM:SS" is 14 chars
            time_ptr += 6; // Move pointer past "time: " to the start of HH:MM:SS
            unsigned long total_seconds = ((time_ptr[0] - '0') * 10UL + (time_ptr[1] - '0')) * 3600UL +
                                          ((time_ptr[3] - '0') * 10UL + (time_ptr[4] - '0')) * 60UL +
                                          ((time_ptr[6] - '0') * 10UL + (time_ptr[7] - '0'));
            message_received_time = total_seconds * 1000UL;
        }
    }
    return {STOREBUFFER, message, message_received_time, nullptr};
}

Message CommandBuffer::parseTime() {
    char timeBuffer[25];
    Time::getFakeHardwareClockTime(timeBuffer, sizeof(timeBuffer));
    Serial.print(F("time: ")); Serial.println(timeBuffer);
    return {TIME, nullptr, 0, nullptr};
}

Message CommandBuffer::parseSendsms(const char* command) {
    // C-string implementation to parse "sendsms [phonenumber] [message]"
    const char* phone_start = strchr(command, ' ');
    if (phone_start == nullptr) {
      Serial.println(F("error: malformed sendsms command. usage: sendsms <number> <message>"));
      return {SENDSMS, nullptr, 0, nullptr};
    }
    phone_start++; // Move past the first space

    const char* message = strchr(phone_start, ' ');
    if (message == nullptr) {
      Serial.println(F("error: malformed sendsms command. missing message."));
      return {SENDSMS, nullptr, 0, nullptr};
    }

    size_t phone_len = message - phone_start;
    if (phone_len > 0 && phone_len < sizeof(CommandBuffer::phonenumber_buffer)) {
      strncpy(CommandBuffer::phonenumber_buffer, phone_start, phone_len);
      CommandBuffer::phonenumber_buffer[phone_len] = '\0'; // Null-terminate the phone number string
      message++; // Move past the space to the actual message
      return {SENDSMS, message, 0, CommandBuffer::phonenumber_buffer};
    }
    
    Serial.println(F("error: invalid phone number."));
    return {SENDSMS, nullptr, 0, nullptr};
}