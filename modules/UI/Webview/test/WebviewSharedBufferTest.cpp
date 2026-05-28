#include "WebviewWrapper.hpp"
#include <Log.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
std::filesystem::path GetExecutableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}
#else
std::filesystem::path GetExecutableDir() {
    return std::filesystem::current_path();
}
#endif

int main() {
    Shin::Log::Init();
    Shin::LOGI("WebviewSharedBufferTest") << "=== Shin Webview Shared Buffer Test ===";

    auto& webview = Shin::UI::WebviewWrapper::GetInstance();

    webview.SetTitle("Shin UI Shared Buffer Test");
    webview.SetSize(800, 600, false);
    
    // Bind C++ function to JS for initialization handshake
    webview.BindFunction("requestSharedBuffer", [&webview](const std::string& req) -> std::string {
        Shin::LOGI("WebviewSharedBufferTest") << "[JS to C++] Frontend is ready! Request: " << req;
        
        size_t bufferSize = 1024 * 1024; // 1MB persistent memory
        int shmId = -1;
        void* ptr = webview.CreateSharedMemory(shmId, bufferSize);

        if (ptr && shmId != -1) {
            Shin::LOGI("WebviewSharedBufferTest") << "Successfully created persistent Shared Memory (" << bufferSize << " bytes), id=" << shmId;
            
            // 0. Initialize atomic lock at the start of the memory (first 4 bytes)
            std::atomic<int32_t>* lock = reinterpret_cast<std::atomic<int32_t>*>(ptr);
            lock->store(0, std::memory_order_relaxed); // 0 = free, 1 = C++ writing, 2 = JS reading

            // 1. Write Initial Data after the 4-byte lock
            char* dataPtr = static_cast<char*>(ptr) + sizeof(int32_t);
            const char* initialData = "Hello from C++ Persistent Shared Memory! (Initial)";
            memcpy(dataPtr, initialData, strlen(initialData) + 1);

            // 2. Post the Handle to JS (Only needed ONCE)
            if (webview.PostSharedMemoryToWeb(shmId, "{\"type\":\"InitialData\", \"message\":\"Handshake success\"}")) {
                Shin::LOGI("WebviewSharedBufferTest") << "Posted Shared Buffer to Script.";

                // 3. Simulate C++ updating the memory constantly
                std::thread([&webview, ptr]() {
                    int counter = 0;
                    std::atomic<int32_t>* lock = reinterpret_cast<std::atomic<int32_t>*>(ptr);
                    char* dataPtr = static_cast<char*>(ptr) + sizeof(int32_t);

                    while (true) {
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        counter++;
                        
                        std::string msg = "Update #" + std::to_string(counter) + " from C++! (True Zero-Copy + Atomics)";
                        
                        // --- Acquire Lock (C++ side) ---
                        int32_t expected = 0;
                        while (!lock->compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
                            expected = 0;
                            std::this_thread::yield(); // Spin wait
                        }
                        
                        // Overwrite the existing memory safely!
                        memcpy(dataPtr, msg.c_str(), msg.length() + 1);
                        
                        // --- Release Lock ---
                        lock->store(0, std::memory_order_release);
                        
                        // Notify JS via lightweight normal IPC
                        webview.SendJson("{\"type\":\"MemoryUpdated\"}");
                        Shin::LOGI("WebviewSharedBufferTest") << "[Background] Sent update " << counter;
                    }
                }).detach();
                
            } else {
                Shin::LOGE("WebviewSharedBufferTest") << "Failed to post Shared Buffer.";
            }
        } else {
            Shin::LOGE("WebviewSharedBufferTest") << "Failed to create Shared Memory.";
        }

        return "{\"status\": \"ok\", \"info\": \"Shared buffer posted\"}";
    });

    auto htmlPath = GetExecutableDir() / "WebviewSharedBufferTest.html";
    std::string url = "file:///" + htmlPath.generic_string();
    Shin::LOGI("WebviewSharedBufferTest") << "Loading URL: " << url;
    webview.SetStartupURL(url);

    if (!webview.Initialize()) {
        Shin::LOGE("WebviewSharedBufferTest") << "Failed to initialize Webview!";
        return -1;
    }

    Shin::LOGI("WebviewSharedBufferTest") << "[Main] Starting UI message loop (blocking)...";
    webview.RunBlocking();
    Shin::LOGI("WebviewSharedBufferTest") << "[Main] UI loop exited successfully.";

    return 0;
}