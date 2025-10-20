#include <cstdlib>
#include <cstring>
#include <Arduino.h>
#include "messagebuffer.h"

MessageBuffer::MessageBuffer() {
  buffer[0] = '\0';      // Initialize buffer to an empty string
  buffer_old[0] = '\0';  // Initialize buffer_old 
}

void MessageBuffer::copyToBuffer(const char* message) {
  strncpy(buffer, message, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';  // GUARANTEE null termination
}

int MessageBuffer::sizeOfBuffer() {
  return sizeof(buffer);
}

void MessageBuffer::printBuffer() {
  if (strcmp(buffer, buffer_old) != 0 && strlen(buffer) > 0) {
    Serial.print(F("buffer: \""));
    Serial.print(buffer);
    Serial.println("\"");
    strcpy(buffer_old, buffer);
  }
}

char* MessageBuffer::c_str() {
  return buffer;
}