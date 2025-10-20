class MessageBuffer {  
  
    char buffer[161];  // Max SMS length + null terminator
    char buffer_old[161];

public:
    MessageBuffer();

    void copyToBuffer(const char* message);
    int sizeOfBuffer();
    void printBuffer();
    char* c_str();
};