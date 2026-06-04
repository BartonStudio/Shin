#include "Log.hpp"
#include <string>

// Define the TAG for this module
static constexpr const char* TAG = "MainApp";

int main() {
    // Initialize the logger
    Shin::Log::Init();

    // Test Callback Sink
    bool callbackCalled = false;
    Shin::Log::AddCallbackSink([&](Shin::LogLevel level, const char* tag, const char* msg) {
        callbackCalled = true;
        printf("--- Callback Received: [%s] %s ---\n", tag, msg);
    });

    // Test JSON Callback Sink
    Shin::Log::AddJsonCallbackSink([](const std::string& jsonMsg, const Shin::Log::LogEntry& entry) {
        printf("--- JSON Received ---\n%s\n---------------------\n", jsonMsg.c_str());
        printf("Structured: [%s] [%s] %s\n", entry.levelStr.c_str(), entry.tag.c_str(), entry.message.c_str());
    });

    // Basic Usage
    Shin::LOGI(TAG) << "ShinLog initialized successfully.";
    
    if (callbackCalled) {
        printf("SUCCESS: Callback was triggered!\n");
    } else {
        printf("FAILURE: Callback was NOT triggered!\n");
        return 1;
    }

    Shin::LOGW(TAG) << "This is a warning message.";
    Shin::LOGE(TAG) << "Simulating an error!";

    // ==========================================
    // Formatting Examples (C++ stream style)
    // ==========================================
    
    // 1. Print integer
    int answer = 42;
    Shin::LOGD(TAG) << "The answer to life is: " << answer;
    
    // 2. Print float/double
    float fps = 59.94f;
    double ms = 16.6666;
    Shin::LOGD(TAG) << "Performance: " << fps << " FPS, " << ms << " ms per frame";
    
    // 3. Print std::string directly (no .c_str() needed!)
    std::string username = "Alice";
    Shin::LOGI(TAG) << "User '" << username << "' has logged in.";

    // 4. Print pointer / memory address
    void* ptr = &answer;
    Shin::LOGD(TAG) << "Variable 'answer' is at memory address: " << ptr;

    // 5. Complex combination
    std::string taskName = "upload";
    int progress = 85;
    size_t bytes = 1024576;
    Shin::LOGI(TAG) << "Task [" << taskName << "] running... Progress: " << progress 
                    << "%, Processed: " << bytes << " bytes";

    return 0;
}