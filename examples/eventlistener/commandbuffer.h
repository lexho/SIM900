#include <cstdint>

enum MessageType { INVALID, EMPTY, STOREBUFFER, TIME, SENDSMS };

typedef struct _Message {
    MessageType type;
    const char* message;
    unsigned long message_received_time;
    const char* phonenumber;
} Message;

class CommandBuffer {
    static const uint8_t serialCmdBufferSize = 255;
    char serialCmdBuffer[serialCmdBufferSize];
    uint8_t serialCmdBufferIdx = 0;
    static const char* storebuffer_cmd;
    static char phonenumber_buffer[20]; // Writable buffer for the phone number

    // These are implementation details, better to keep them private
    Message parseStorebuffer(const char* command);
    Message parseTime();
    Message parseSendsms(const char* command);
public:
    Message parseCommand(const char* command);
    Message read(char receivedChar);
};