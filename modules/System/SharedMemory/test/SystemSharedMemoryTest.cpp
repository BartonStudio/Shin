#include "SharedMemory.hpp"
#include <Log.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <string>

int main() {
    Shin::LOGI("SharedMemoryTest") << "=== Shin SharedMemory Test ===";

    // 1. Create Shared Memory (Name: "ShinTestMap", Size: 1MB)
    Shin::System::SharedMemory shm("ShinTestMap", 1024 * 1024);
    
    Shin::LOGI("Main") << "Created shared memory: " << shm.GetName() 
              << " (Size: " << shm.GetSize() << " bytes)";

    // 2. Start Background Thread (Simulate Python or Node.js)
    std::thread writer([&]() {
        Shin::LOGI("Writer") << "Thread started, waiting 1s...";
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Scenario A: Easy Write API
        std::string msg = "Hello from Writer Thread (simulating Python)!";
        if (shm.Write(0, msg.c_str(), msg.size() + 1)) {
            Shin::LOGI("Writer") << "Successfully wrote data using Write().";
        } else {
            Shin::LOGE("Writer") << "Failed to write data.";
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Scenario B: Advanced Zero-Copy Write
        if (shm.Lock(100)) {
            std::string msg2 = "Advanced zero-copy write completed!";
            memcpy(static_cast<char*>(shm.GetBuffer()) + 100, msg2.c_str(), msg2.size() + 1);
            shm.Unlock();
            Shin::LOGI("Writer") << "Successfully wrote advanced data.";
        }
    });

    // 3. Main Thread Reader
    Shin::LOGI("Reader") << "Trying to read data...";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    // Scenario A: Easy Read API
    char readBuf[256] = {0};
    if (shm.Read(0, readBuf, sizeof(readBuf))) {
        Shin::LOGI("Reader") << "Read data via Read(): " << readBuf;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Scenario B: Advanced Zero-Copy Read
    if (shm.Lock(100)) {
        char* ptr = static_cast<char*>(shm.GetBuffer()) + 100;
        Shin::LOGI("Reader") << "Advanced Read (Zero-Copy): " << ptr;
        shm.Unlock();
    }

    writer.join();
    Shin::LOGI("Main") << "Test completed successfully.";

    return 0;
}