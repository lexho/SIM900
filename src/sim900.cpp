/*
 * This file is part of the SIM900 Arduino Shield library.
 * Copyright (c) 2023 Nathanne Isip
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "sim900.h"

void SIM900::sendCommand(String message) {
    this->sim900.println(message);
}

String SIM900::getResponse() {
    delay(500);
    
    if(this->sim900.available() > 0) {
        String response = this->sim900.readString();
        response.trim();

        return response;
    }

    return "";
}

bool SIM900::isReady() {
    String response = this->getResponse();
    if(response.indexOf("+CFUN: 1") != - 1 && response.indexOf("+CPIN: READY") != - 1 && response.indexOf("Call Ready") != - 1) {
        return true;
    }
    return false;
}

String SIM900::getReturnedMode() {
    String response = this->getResponse();
    return response.substring(response.lastIndexOf('\n') + 1);
}

bool SIM900::isSuccessCommand() {
    return this->getReturnedMode() == F("OK");
}

String SIM900::rawQueryOnLine(uint16_t line) {
    String response = this->getResponse();
    String result = "";

    uint16_t currentLine = 0;
    for(int i = 0; i < response.length(); i++)
        if(currentLine == line && response[i] != '\n')
            result += response[i];
        else if(response[i] == '\n') {
            currentLine++;

            if(currentLine > line)
                break;
        }

    return result;
}

String SIM900::queryResult() {
    String response = this->getResponse();
    String result = F("");

    int idx = response.indexOf(": ");
    if(idx != -1)
        result = response.substring(
            idx + 2,
            response.indexOf('\n', idx)
        );

    return result;
}

SIM900::SIM900(Stream& _sim900):sim900(_sim900) {
    pinMode(STATUS_LED, OUTPUT);
    pinMode(STATUS_LED_ERROR, OUTPUT);
    pinMode(PWRKEY, OUTPUT);
    pinMode(RST_PIN, OUTPUT);
    calling = false;
    //this->bootstrap //?
}

bool SIM900::bootstrap() {
  digitalWrite(PWRKEY, LOW); // Keep PWRKEY low initially
  digitalWrite(RST_PIN, HIGH); // Keep SIM900 out of reset state
  // led test
  digitalWrite(STATUS_LED, HIGH);
  digitalWrite(STATUS_LED_ERROR, HIGH);
  delay(2000);

  static bool reset_attempted = false;
  int signal_strength = 0; // Declare variable at the top of the scope
  String resp;
  bool simcardOK = false;
  bool signalOK = false;

  digitalWrite(STATUS_LED, LOW);
  digitalWrite(STATUS_LED_ERROR, LOW);
  // stage 0
  digitalWrite(STATUS_LED, HIGH); // stage 1
  digitalWrite(STATUS_LED, LOW);
  
  Serial.println(F("--------------------------"));
  Serial.println(F("Arduino SIM900 SMS WEATHER SERVICE"));
  Serial.println(F("--------------------------"));
  Serial.println(F("[#     ] stage1: status led"));
  Serial.println(F("[##    ] stage2: serial ready")); // stage 2

  Serial.println(F("[###   ] stage3: shield serial seems to be ready")); // stage 3
  delay(1000);
  // can we write to Serial1?
  //Serial.println(F("[###   ] can we write to Serial1?"));
  this->sim900.println(F("AT"));
  //Serial.println(F("[###   ] wrote 'AT'-command to Serial1"));
  //Serial.println(F("[###   ] trying to read from Serial1..."));
  delay(400);
  if(this->sim900.available()) {
    String response;
    if(this->sim900.available() > 0) {
        response =  this->sim900.readString();
        //response.trim();
    }
    //Serial.println();
    //Serial.print(F("[###   ] read ")); Serial.print(response.length()); Serial.println(F(" bytes"));
    // stage 4
    //Serial.print(F("response: ")); Serial.println(response);
    //if(response.length() > 0) Serial.println(F("[###   ] got a response."));
    if(response.indexOf("ERROR") != -1) {
      digitalWrite(STATUS_LED_ERROR, HIGH);
      Serial.println(F("[###   ] did receive an 'AT ERROR'"));
      return false;
    }
    if(response.indexOf("OK") != -1) {
      Serial.println(F("[####  ] stage4: shield serial is ready")); // stage 4
    } else {
      digitalWrite(STATUS_LED_ERROR, HIGH);
      Serial.println(F("[###   ] error: did not receive valid response"));
      Serial.println(F("[###   ] cannot read from shield serial. power down? check power state, check wiring"));
      // reset; if test fails poweron
      goto bootstrap_fail;
    }
  } else {
    Serial.println(F("[###   ] error: No serial data available from SIM900."));
    goto bootstrap_fail;
  }
  delay(3000);

  // stage 5 handshake from library
  if(this->handshake()) {
    Serial.println(F("[##### ] stage5: handshaked!"));
  } else {
    Serial.println(F("[####  ] error: handshake failed."));
    goto bootstrap_fail;
  }

  // stage 6 sim cardok, signal strengthok, net status, receive calls, sms
  this->sendCommand("AT+CPIN?");
  resp = this->getResponse();
  if(resp.indexOf("+CPIN: READY") != -1) {
    simcardOK = true;
    //Serial.println(F("[##### ] sim card is ready"));
  } else {
    digitalWrite(STATUS_LED_ERROR, HIGH);
    Serial.println(F("[##### ] sim card is not ready"));
    return false;
  }

  signal_strength = measureSignalStrength();
  signalOK = isSignalOk(signal_strength);
  if(simcardOK && signalOK) {
    Serial.println(F("[######] stage6: simcard is ready and signal is OK"));
  } else {
    digitalWrite(STATUS_LED_ERROR, HIGH);
    return false;
  }

  reset_attempted = false; // Success, so reset the flag for the next time bootstrap might fail.
  return true;

bootstrap_fail:
  digitalWrite(STATUS_LED_ERROR, HIGH);
  Serial.println(F("bootstrap failed. attempting recovery..."));
  if (!reset_attempted) {
    this->reset();
    reset_attempted = true;
  } else {
    this->powerOn();
  }
  return false;
}

void SIM900::reset()
{
  Serial.println(F("performing soft reset..."));
  digitalWrite(RST_PIN, LOW); // Set the pin LOW to trigger reset
  delay(100);                  // Wait briefly
  digitalWrite(RST_PIN, HIGH);  // Set the pin HIGH to release from reset
  delay(1000);                 // Wait for the module to restart
}

void SIM900::powerOn()
{
  Serial.println(F("performing power cycle..."));
  digitalWrite(PWRKEY, HIGH);
  delay(1000); // Hold PWRKEY for 1 second
  digitalWrite(PWRKEY, LOW);

  Serial.println(F("waiting for sim900 to be available..."));
  delay(10000); // Wait for the module to boot

  if(this->isReady()) Serial.println(F("sim900 is on and ready."));
}

bool SIM900::handshake() {
    this->sendCommand(F("AT"));
    return this->isSuccessCommand();
}

bool SIM900::isCardReady() {
    this->sendCommand(F("AT+CPIN?"));
    return this->isSuccessCommand();
}

bool SIM900::setPhoneNumber(const char* number) {
    strncpy(this->phonenumber, number, sizeof(this->phonenumber) - 1);
    this->phonenumber[sizeof(this->phonenumber) - 1] = '\0'; // Ensure null termination
    if(sizeof(this->phonenumber) > 0) return true;
    return false;
}

bool SIM900::changeCardPin(uint8_t pin) {
    if(pin > 9999)
        return false;

    this->sendCommand("AT+CPIN=\"" + String(pin) + "\"");
    return this->isSuccessCommand();
}

const char* SIM900::extract(char* line, const char delim) {
    char* start = strchr(line, delim);
    if (start) {
        start++; // Move past the opening delimiter
        char* end = strchr(start, delim);
        if (end) {
            *end = '\0'; // Terminate the substring
            return start;
        }
    }
    return "";
}

SIM900_SMS SIM900::sms;

const char* SIM900::getPhoneNumber() {
  return phonenumber;
}

void SIM900::setLastRingMessageTime(unsigned long time) {
  lastRingMessageTime = time;
}

bool SIM900::isCalling() {
  return calling;
}

void SIM900::clearBuffer() {
  while(this->sim900.available()) { this->sim900.read(); }
}

void SIM900::registerListener(std::unique_ptr<EventListener> listener)  {
    this->listeners.push_back(std::move(listener));
}

SIM900_Handler_Event SIM900::handleEvents() {
    const char* OK_REPLY = "OK";
    const char* RING_REPLY = "RING";
    const char* NO_CARRIER_REPLY = "NO CARRIER";
    const char* CMT_REPLY = "+CMT:";
    const char* CMGS_REPLY = "+CMGS"; // Check for +CMGS without the colon for broader compatibility
    const char* CLIP_REPLY = "+CLIP: ";

    SIM900_Handler_Event handlerState;
    handlerState.status = SIM900_NOTHING;

    // Use a static buffer to avoid heap fragmentation from String concatenation
    static char msgBuffer[160]; // 128
    static uint8_t msgIdx = 0;

    //msg = sim900.readStringUntil('\n');
    // read from sim900 line by line
    // avoiding inefficient substring() and getResponse() calls
    while(this->sim900.available()) {
        char ch = this->sim900.read();

        // Process the buffer when a newline is received, indicating end of a line.
        if (ch == '\n') {
            if (msgIdx > 0) { // We have a complete line to process
                msgBuffer[msgIdx] = '\0'; // Null-terminate the buffer to make it a valid C-string

        if (strcmp(msgBuffer, RING_REPLY) == 0) {
          handlerState.status = SIM900_RING;
          if (!this->listeners.empty()) {
            for(const auto& listener : this->listeners) {
            if(listener->type == SIM900_RING) { listener->execute(); }
            }
          }
        } else if (strcmp(msgBuffer, OK_REPLY) == 0) {
          handlerState.status = SIM900_OK;
          msgIdx = 0;
          if (!this->listeners.empty()) {
            for(const auto& listener : this->listeners) {
              if(listener->type == SIM900_OK) { listener->execute(); }
            }
          }
          return handlerState;
        } else if (strcmp(msgBuffer, NO_CARRIER_REPLY) == 0) {
          handlerState.status = SIM900_NOCARRIER;
          msgIdx = 0;
          if (!this->listeners.empty()) {
            for(const auto& listener : this->listeners) {
              if(listener->type == SIM900_NOCARRIER) { listener->execute(); }
            }
          }
          return handlerState;
        } else if (strstr(msgBuffer, CMGS_REPLY) != NULL) {
          handlerState.status = SIM900_CMGS;
          msgIdx = 0;
          return handlerState;
        } else if (strstr(msgBuffer, CLIP_REPLY) != NULL) {
          const char* phonenr = this->extract(msgBuffer, '"');
          this->setPhoneNumber(phonenr);
          handlerState.phonenumber = phonenr;
          handlerState.status = SIM900_CLIP;
          // Don't return yet, RING might be followed by other data.
        } else if (strstr(msgBuffer, CMT_REPLY) != NULL) {
          const char* phonenr = this->extract(msgBuffer, '"');
          this->setPhoneNumber(phonenr);
          handlerState.phonenumber = phonenr;
          handlerState.status = SIM900_CMT;
          if (!this->listeners.empty()) {
            for(const auto& listener : this->listeners) {
              if(listener->type == SIM900_CMT) { listener->execute(); }
            }
          }
          msgIdx = 0;
          return handlerState;
        }
      }
      msgIdx = 0;
    } else if (ch != '\r' && msgIdx < sizeof(msgBuffer) - 1) {
      msgBuffer[msgIdx++] = ch;
    }
  }

  if (handlerState.status != SIM900_NOTHING) {
    return handlerState;
  }

  return handlerState;
}

SIM900_SMS SIM900::readSMS() {
    return this->sms;
}

String SIM900::readSMSFromSIM() {
    // phone number, message
    //this->sendCommand("AT+CMGF=1");
    //delay(100);
    //this->sendCommand("AT+CNMI=2,2,0,0,0");
    //delay(100);
    this->sendCommand("AT+CMGL=\"REC UNREAD\"");
    delay(100);
    //this->sendCommand("AT+CMGR=1");
    //delay(100);
    //this->sendCommand("AT+CMGD=1,4");

    String str = sim900.readStringUntil('\n');
    Serial.println(str);
    //  AT+CMGR=1\r
    //  AT+CMGR=2\r
    // AT+CMGR=ALL\r

    return str;
}

int SIM900::rssiToDbm(int rssi) {
  if (rssi == 99) {
    return 999; // Represents "not known or not detectable"
  }
  if (rssi == 0) {
    return -113;
  }
  if (rssi == 1) {
    return -111;
  }
  if (rssi == 31) {
    return -51;
  }
  if (rssi >= 2 && rssi <= 30) {
    // Linear conversion for the main range
    return -113 + (rssi * 2);
  }
  
  return 999; // Return an error code for any other value
}

int SIM900::measureSignalStrength() {
  SIM900Signal signal = this->signal(); // 0 - 31
  int signal_rssi = signal.rssi;
  digitalWrite(SIGNAL_LED1, LOW); digitalWrite(SIGNAL_LED2, LOW); digitalWrite(SIGNAL_LED3, LOW);
  if(signal_rssi >= 0) { digitalWrite(SIGNAL_LED1, HIGH); }
  if(signal_rssi >= 11) { digitalWrite(SIGNAL_LED2, HIGH); }
  if(signal_rssi >= 21) { digitalWrite(SIGNAL_LED3, HIGH); }
  //Serial.print(signal_rssi); // -113dBm to -51dBm
  int signal_strength = rssiToDbm(signal_rssi);
  //Serial.print(F("signal strength: "));
  //Serial.print(signal_strength); // -113dBm to -51dBm
  //Serial.println(F("dBm"));
  if (signal_rssi >= 2 && signal_rssi < 10) {
    //Serial.println(F("signal strength is marginal."));
  }
  if (signal_rssi >= 10 && signal_rssi <= 30) {
    //Serial.println(F("signal strength is OK."));
  }
  if(signal_rssi == 0 || signal_rssi == 1 || signal_rssi == 31) {
    //Serial.println(F("bad signal"));
  }
  return signal_rssi;
}

bool SIM900::isSignalOk(int signal_strength) {
  if(signal_strength >= 10 && signal_strength <= 30) return true;
  else return false;
}


SIM900Signal SIM900::signal() {
    SIM900Signal signal;
    signal.rssi = signal.bit_error_rate = 0;
    this->sendCommand("AT+CSQ");

    String response = this->queryResult();
    if (response.length() == 0) {
            return signal;
    }
    
    // split values separated by comma "15,0"
    const char* response_cstr = response.c_str(); // the start of the string; strchr(response, ','); // 0
    const char* comma = strchr(response_cstr, ',');
    if (comma != NULL) {
        signal.rssi = atoi(response_cstr);
        signal.bit_error_rate = atoi(comma + 1);
    }

    //signal.rssi = (uint8_t) response.substring(0, delim).toInt();
    //signal.bit_error_rate = (uint8_t) response.substring(delim + 1).toInt();

    return signal;
}

// void SIM900::close() {
//     this->sim900->end();
// }

SIM900DialResult SIM900::dialUp(String number) {
    this->sendCommand("ATD+ " + number + ";");

    SIM900DialResult result = SIM900_DIAL_RESULT_ERROR;
    String mode = this->getReturnedMode();

    if(mode == F("NO DIALTONE"))
        result = SIM900_DIAL_RESULT_NO_DIALTONE;
    else if(mode == F("BUSY"))
        result = SIM900_DIAL_RESULT_BUSY;
    else if(mode == F("NO CARRIER"))
        result = SIM900_DIAL_RESULT_NO_CARRIER;
    else if(mode == F("NO ANSWER"))
        result = SIM900_DIAL_RESULT_NO_ANSWER;
    else if(mode == F("OK"))
        result = SIM900_DIAL_RESULT_OK;

    return result;
}

SIM900DialResult SIM900::redialUp() {
    this->sendCommand(F("ATDL"));

    SIM900DialResult result = SIM900_DIAL_RESULT_ERROR;
    String mode = this->getReturnedMode();

    if(mode == F("NO DIALTONE"))
        result = SIM900_DIAL_RESULT_NO_DIALTONE;
    else if(mode == F("BUSY"))
        result = SIM900_DIAL_RESULT_BUSY;
    else if(mode == F("NO CARRIER"))
        result = SIM900_DIAL_RESULT_NO_CARRIER;
    else if(mode == F("NO ANSWER"))
        result = SIM900_DIAL_RESULT_NO_ANSWER;
    else if(mode == F("OK"))
        result = SIM900_DIAL_RESULT_OK;

    return result;
}

SIM900DialResult SIM900::acceptIncomingCall() {
    this->sendCommand(F("ATA"));

    SIM900DialResult result = SIM900_DIAL_RESULT_ERROR;
    String mode = this->getReturnedMode();

    if(mode == F("NO CARRIER"))
        result = SIM900_DIAL_RESULT_NO_CARRIER;
    else if(mode == F("OK"))
        result = SIM900_DIAL_RESULT_OK;

    return result;
}

bool SIM900::hangUp() {
    this->sendCommand(F("ATH"));
    return this->isSuccessCommand();
}

void SIM900::printResponse(String response) {
    Serial.print("response: \"");
    Serial.print(response);
    Serial.println("\"");
}

String SIM900::readLine() {
    if(!this->sim900.available()) return "";
    int timeout = 1000;
    int start = millis();
    char ch = ' ';
    String str = String();
    while(this->sim900.available() && ch != '\n') {
        ch = this->sim900.read();
        //Serial.print(ch);
        str += ch;
        if((millis() - start) > timeout) break;
    }
    //Serial.println();
    delay(100);
    return str;
}

bool SIM900::sendSMSRoutine(const char* phonenumber, const char* message) {
  bool validPhoneNumber = true;
  bool validMessage = true;
  if(strstr(phonenumber, "+43") == NULL) {
    Serial.println(F("invalid phonenumber. only phonenumber from Austria are allowed."));
    validPhoneNumber = false;
    return false;
  }
  const int max_retries = 2;
  for(int retries = max_retries; retries > 0; retries--) {
    delay(200);
    if (!this->handshake()) {
      Serial.println(F("handshake failed, retrying..."));
      continue; // Skip to the next attempt
    }

    this->sim900.readString(); // Clear any lingering response from the buffer
    Serial.println(F("sending sms..."));
    Serial.print(F("phonenumber: \"")); Serial.print(phonenumber); Serial.println(F("\""));

    bool sent = false;
    if(strlen(message) > 0) {
      Serial.print(F("message: ")); Serial.println(message);
      sent = this->sendSMS(phonenumber, message); // Convert to String for the library function
    } else {
      Serial.println(F("no message to send."));
      validMessage = false;
    }

    if (sent) {
      return true; // Success! Exit the function.
    }
  }
  if(validPhoneNumber && validMessage) this->bootstrap(); // reset SIM900
  return false;
}

bool SIM900::sendSMSRoutine(const char* message) {
  this->sendSMSRoutine(this->phonenumber, message);
}

bool SIM900::sendSMS(const char* number, const char* message) {
    // 1. Set SMS text mode and wait for "OK"
    this->sendCommand(F("AT+CMGF=1"));
    if(!this->isSuccessCommand()) return false;

    // 2. Set character set to GSM and wait for "OK"
    this->sendCommand(F("AT+CSCS=\"GSM\""));
    if(!this->isSuccessCommand()) return false;

    // 3. Send phone number
    char command[40];
    snprintf(command, sizeof(command), "AT+CMGS=\"%s\"", number);
    this->sendCommand(command);

    // 4. Wait for the ">" prompt
   String response = this->getResponse();
    if (response.indexOf('>') == -1) return false; // Didn't get prompt
    delay(100);
    unsigned long startTime = millis();
    while (millis() - startTime < 3000) { // 2-second timeout
        if (this->sim900.peek() == '>') {
            this->sim900.read(); // Consume the '>'
            // 5. Send message content and Ctrl+Z
            this->sim900.print(message);
            this->sim900.write(0x1A);
            return true; // Assume success after sending Ctrl+Z
        }
    }

    // 5. Send message content and Ctrl+Z
    this->sim900.print(message);
    delay(500);
    this->sim900.write(0x1A);
    Serial.println("message and ctrl-z sent, waiting for confirmation...");

    // 6. Wait for +CMGS confirmation or ERROR
    // The module can take several seconds to send the SMS
    startTime = millis();
    while (millis() - startTime < 10000) { // 10-second timeout
        if (this->sim900.available()) {
            String line = this->sim900.readStringUntil('\n');
            line.trim();
            //Serial.print("\"");
            //Serial.print(line);
            //Serial.println("\"");
            if (line.startsWith(F("+CMGS:"))) {
                Serial.println("sms sent successfully.");
                return true; // Success!
            }
            if (line.startsWith(F("+CMS ERROR:")) || line.startsWith(F("ERROR"))) {
                Serial.println("sms failed to send (ERROR).");
                return false; // Failure
            }
        }
    }

    Serial.println("timed out waiting for sms confirmation.");
    return false; // Timed out
}

bool SIM900::sendSMS2(String number, String message) {
    String response;
    // Set SMS to PDU mode
    this->sendCommand(F("AT+CMGF=0"));
    response = sim900.readStringUntil('\n');
    delay(500);

    // Convert message to UCS2 hex string
    String pduMessage = "";
    for (int i = 0; i < message.length(); i++) {
        char highByte = (message[i] >> 8) & 0xFF;
        char lowByte = message[i] & 0xFF;

        if (highByte < 0x10) pduMessage += '0';
        pduMessage += String(highByte, HEX);
        if (lowByte < 0x10) pduMessage += '0';
        pduMessage += String(lowByte, HEX);
    }

    // The PDU string needs to be built. This is a simplified example for UCS2.
    // A full PDU implementation is more complex.
    // For now, we can try a simpler method with Text Mode and UCS2 character set.
    this->sendCommand(F("AT+CMGF=1")); // Back to Text Mode
    response = sim900.readStringUntil('\n');
    Serial.println(response);
    delay(500);
    //this->isSuccessCommand(); // Consume OK
    this->sendCommand(F("AT+CSCS=\"UCS2\"")); // Set character set to UCS2
    response = sim900.readStringUntil('\n');
    Serial.println(response);
    delay(500);
    //this->isSuccessCommand(); // Consume OK

    this->sendCommand("AT+CMGS=\"" + number + "\"");
    delay(500); // Wait for '>'

    this->sim900.print(pduMessage);
    this->sim900.write(0x1A); // End of message character (Ctrl+Z)
    delay(500);

    while(sim900.available()) {
        response = sim900.readStringUntil('\n');
        Serial.println(response);
        delay(100);
    }

    return this->isSuccessCommand();
}

SIM900Operator SIM900::networkOperator() {
    SIM900Operator simOperator;
    simOperator.mode = static_cast<SIM900OperatorMode>(0);
    simOperator.format = static_cast<SIM900OperatorFormat>(0);
    simOperator.name = "";

    this->sendCommand(F("AT+COPS?"));

    String response = this->queryResult();
    uint8_t delim1 = response.indexOf(','),
        delim2 = response.indexOf(',', delim1 + 1);

    simOperator.mode = intToSIM900OperatorMode((uint8_t) response.substring(0, delim1).toInt());
    simOperator.format = intToSIM900OperatorFormat((uint8_t) response.substring(delim1 + 1, delim2).toInt());
    simOperator.name = response.substring(delim2 + 2, response.length() - 2);

    return simOperator;
}

bool SIM900::connectAPN(SIM900APN apn) {
    this->sendCommand(F("AT+CMGF=1"));
    if(!this->isSuccessCommand())
        return false;

    this->sendCommand(F("AT+CGATT=1"));
    if(!this->isSuccessCommand())
        return false;
    
    this->sendCommand(
        "AT+CSTT=\"" + apn.apn +
        "\",\"" + apn.username +
        "\",\"" + apn.password + "\""
    );

    return (this->hasAPN = this->isSuccessCommand());
}

bool SIM900::enableGPRS() {
    if(!this->hasAPN)
        return false;

    this->sendCommand(F("AT+CIICR"));
    delay(1000);

    return this->isSuccessCommand();
}

SIM900HTTPResponse SIM900::request(SIM900HTTPRequest request) {
    SIM900HTTPResponse response;
    response.status = -1;

    if(!this->hasAPN)
        return response;

    this->sendCommand(
        "AT+CIPSTART=\"TCP\",\"" + request.domain +
        "\"," + String(request.port)
    );
    
    String resp = this->getResponse();
    resp.trim();

    delay(1500);
    if(!resp.endsWith(F("CONNECT OK")))
        return response;

    String requestStr = request.method + " " +
        request.resource + " HTTP/1.0\r\nHost: " +
        request.domain + "\r\n";

    for(int i = 0; i < request.header_count; i++)
        requestStr += request.headers[i].key + ": " +
            request.headers[i].value + "\r\n";

    if(request.data != "" || request.data != NULL)
        requestStr += request.data + "\r\n";

    requestStr += F("\r\n");
    this->sendCommand(requestStr);

    // TODO
    return response;
}

bool SIM900::updateRtc(SIM900RTC config) {
    // Use a char buffer and snprintf for memory-safe string formatting.
    // This avoids heap fragmentation caused by String concatenation.
    char command[50]; // Buffer to hold the AT command string.

    // Format: AT+CCLK="YY/MM/DD,hh:mm:ss+TZ"
    // The %02d format specifier handles zero-padding for all date/time parts.
    // The %+d format specifier for GMT ensures a sign (+ or -) is always included.
    snprintf(command, sizeof(command),
             "AT+CCLK=\"%02d/%02d/%02d,%02d:%02d:%02d%+d\"",
             config.year, config.month, config.day,
             config.hour, config.minute, config.second,
             config.gmt
    );

    this->sendCommand(command);
    return this->isSuccessCommand();
}

SIM900RTC SIM900::rtc() {
    SIM900RTC rtc;
    rtc.year = rtc.month = rtc.day =
        rtc.hour = rtc.minute = rtc.second = 
        rtc.gmt = 0;

    this->sendCommand(F("AT+CMGF=1"));
    if(!this->isSuccessCommand())
        return rtc;

    this->sendCommand(F("AT+CENG=3"));
    if(!this->isSuccessCommand())
        return rtc;
    this->sendCommand(F("AT+CCLK?"));

    // queryResult() returns a string like: "24/05/15,10:30:00+08"
    String time_str = this->queryResult();
    if (time_str.length() == 0) {
        return rtc;
    }

    // strtok modifies the string, so we need a non-const char array.
    // Let's copy the relevant part of the string into a temporary buffer.
    char buffer[25]; // "YY/MM/DD,hh:mm:ss+TZ" is 20 chars + quotes + null
    strncpy(buffer, time_str.c_str() + 1, sizeof(buffer) - 1); // +1 to skip opening quote
    buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination

    // Use strtok to tokenize the string. Delimiters are /, ,, :, and +
    char* token = strtok(buffer, "/,:+");
    if (token != NULL) rtc.year = atoi(token);
    token = strtok(NULL, "/,:+");
    if (token != NULL) rtc.month = atoi(token);
    token = strtok(NULL, "/,:+");
    if (token != NULL) rtc.day = atoi(token);
    token = strtok(NULL, "/,:+");
    if (token != NULL) rtc.hour = atoi(token);
    token = strtok(NULL, "/,:+");
    if (token != NULL) rtc.minute = atoi(token);
    token = strtok(NULL, "/,:+");
    if (token != NULL) rtc.second = atoi(token);
    token = strtok(NULL, "/,:+");
    if (token != NULL) rtc.gmt = atoi(token);

    return rtc; 
}

bool SIM900::savePhonebook(uint8_t index, SIM900CardAccount account) {
    this->sendCommand(
        "AT+CPBW=" + String(index) +
        ",\"" + account.number +
        "\"," + account.numberType +
        ",\"" + account.name + "\""
    );
    return this->isSuccessCommand();
}

SIM900CardAccount SIM900::retrievePhonebook(uint8_t index) {
    this->sendCommand("AT+CPBR=" + String(index));

    SIM900CardAccount accountInfo;
    accountInfo.numberType = static_cast<SIM900PhonebookType>(0);

    String response = this->queryResult();
    response = response.substring(response.indexOf(',') + 1);

    uint8_t delim1 = response.indexOf(','),
        delim2 = response.indexOf(',', delim1 + 1);

    accountInfo.number = response.substring(1, delim1 - 1);
    
    uint8_t type = (uint8_t) response.substring(delim1 + 1, delim2).toInt();
    if(type == 129 || type == 145)
        accountInfo.numberType = static_cast<SIM900PhonebookType>(type);
    else accountInfo.numberType = static_cast<SIM900PhonebookType>(0);

    accountInfo.name = response.substring(delim2 + 2, response.length() - 2);
    return accountInfo;
}

bool SIM900::deletePhonebook(uint8_t index) {
    this->sendCommand("AT+CPBW=" + String(index));
    return this->isSuccessCommand();
}

SIM900PhonebookCapacity SIM900::phonebookCapacity() {
    SIM900PhonebookCapacity capacity;
    capacity.used = capacity.max = 0;
    capacity.memoryType = F("");

    this->sendCommand("AT+CPBS?");

    String response = this->queryResult();
    uint8_t delim1 = response.indexOf(','),
        delim2 = response.indexOf(',', delim1 + 1);

    capacity.memoryType = response.substring(1, delim1 - 1);
    capacity.used = (uint8_t) response.substring(delim1 + 1, delim2).toInt();
    capacity.max = (uint8_t) response.substring(delim2 + 1).toInt();

    return capacity;
}

SIM900CardAccount SIM900::cardNumber() {
    this->sendCommand(F("AT+CNUM"));

    SIM900CardAccount account;
    account.name = F("");

    String response = this->queryResult();
    if(response == F(""))
        return account;

    uint8_t delim1 = response.indexOf(','),
        delim2 = response.indexOf(',', delim1 + 1),
        delim3 = response.indexOf(',', delim2 + 1),
        delim4 = response.indexOf(',', delim3 + 1);

    account.name = response.substring(1, delim1 - 1);
    account.number = response.substring(delim1 + 2, delim2 - 1);
    account.type = (uint8_t) response.substring(delim2 + 1, delim3).toInt();
    account.speed = (uint8_t) response.substring(delim3 + 1, delim4).toInt();
    account.service = intToSIM900CardService((uint8_t) response.substring(delim4 + 1).toInt());
    account.numberType = static_cast<SIM900PhonebookType>(0);

    return account;
}

String SIM900::manufacturer() {
    this->sendCommand(F("AT+GMI"));
    return this->rawQueryOnLine(2);
}

String SIM900::softwareRelease() {
    this->sendCommand(F("AT+GMR"));

    String result = this->rawQueryOnLine(2);
    result = result.substring(result.lastIndexOf(F(":")) + 1);

    return result;
}

String SIM900::imei() {
    this->sendCommand(F("AT+GSN"));
    return this->rawQueryOnLine(2);
}

String SIM900::chipModel() {
    this->sendCommand(F("AT+GMM"));
    return this->rawQueryOnLine(2);
}

String SIM900::chipName() {
    this->sendCommand(F("AT+GOI"));
    return this->rawQueryOnLine(2);
}

String SIM900::ipAddress() {
    this->sendCommand(F("AT+CIFSR"));
    return this->rawQueryOnLine(2);
}