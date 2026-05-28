#include "WebviewWrapper.hpp"
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
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
std::filesystem::path GetExecutableDir() {
    char path[1024];
    uint32_t size = sizeof(path);
#if defined(__APPLE__)
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::filesystem::path(path).parent_path();
    }
#else
    if (readlink("/proc/self/exe", path, size) != -1) {
        return std::filesystem::path(path).parent_path();
    }
#endif
    return std::filesystem::current_path();
}
#endif

int main() {
    std::cout << "=== Shin Webview UI Test ===" << std::endl;

    auto& webview = Shin::UI::WebviewWrapper::GetInstance();

    // 1. Setup Phase
    webview.SetTitle("Shin UI Multi-Thread Test");
    webview.SetSize(800, 600, false);
    
    // Bind C++ function to JS
    webview.BindFunction("cppLog", [](const std::string& req) -> std::string {
        std::cout << "[JS to C++] Button clicked! Data from JS: " << req << std::endl;
        return "{\"status\": \"ok\"}";
    });

    // Dynamically build path to MultiThreadEventTest.html in the same directory
    auto htmlPath = GetExecutableDir() / "MultiThreadEventTest.html";
    std::string url = "file:///" + htmlPath.generic_string();
    std::cout << "Loading URL: " << url << std::endl;
    webview.SetStartupURL(url);

    // 2. Initialize
    if (!webview.Initialize()) {
        std::cerr << "Failed to initialize Webview!" << std::endl;
        return -1;
    }

    // 3. Thread Safety Test (Background Task Simulation)
    std::thread bgThread([&]() {
        std::cout << "[Background] Thread started. Emitting time infinitely..." << std::endl;
        
        // Infinite loop: will keep sending data until the main process exits
        while (true) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::string timeStr = std::ctime(&now_c);
            if (!timeStr.empty() && timeStr.back() == '\n') {
                timeStr.pop_back(); // Remove newline added by ctime
            }
            
            std::string jsonPayload = "{\"type\": \"UpdateStatus\", \"message\": \"Current Time: " + timeStr + "\"}";
            webview.SendJson(jsonPayload);
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    bgThread.detach();

    // 4. Run Blocking
    std::cout << "[Main] Starting UI message loop (blocking)..." << std::endl;
    webview.RunBlocking();
    std::cout << "[Main] UI loop exited successfully." << std::endl;

    return 0;
}
