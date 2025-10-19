// Declare the EventListener base class
class EventListener {
public:
    // Virtual destructor is crucial for proper cleanup of derived objects through base pointers
    virtual ~EventListener() = default;
    virtual bool execute() {
        // Default implementation, can be overridden by derived classes
        // Removed std::cout as it's not standard Arduino practice
        return true;
    }
    int type = 0; // Member to identify event type
};