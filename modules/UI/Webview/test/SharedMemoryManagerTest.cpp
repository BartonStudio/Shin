#include "WebviewWrapper.hpp"
#include "WebviewMessageHandler.hpp"
#include <Log.hpp>
#include <string>
#include <filesystem>
#include <iostream>
#include <thread>
#include <algorithm>

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
    Shin::LOGI("Main") << "=== Shin Shared Memory Manager Test ===";

    auto& webview = Shin::UI::WebviewWrapper::GetInstance();

    webview.SetTitle("Shin UI Shared Memory Manager Test");
    webview.SetSize(800, 600, false);
    
    // 开启 Webview 调试模式
    webview.SetDebug(true);

    auto htmlPath = GetExecutableDir() / "SharedMemoryManagerTest.html";
    std::string url = "file:///" + htmlPath.generic_string();
    Shin::LOGI("Main") << "Loading URL: " << url;
    webview.SetStartupURL(url);

    if (!webview.Initialize()) {
        Shin::LOGE("Main") << "Failed to initialize Webview!";
        return -1;
    }

    // ==========================================
    // 将业务代码（消费 JS 推送）移出 MessageHandler
    // ==========================================
    Shin::UI::WebviewMessageHandler::AddSharedMemoryUpdateObserver([&webview](int id) {
        void* ptr = webview.GetSharedMemory(id);
        if (ptr) {
            size_t size = webview.GetSharedMemorySize(id);
            std::string content(static_cast<const char*>(ptr), strnlen(static_cast<const char*>(ptr), size));
            Shin::LOGI("BusinessLayer") << "[Observer] 捕获到 JS 修改了内存 ID " << id << "，最新内容为: " << content;
        }
    });

    // 启动一个后台线程用于在终端接受 C++ 输入，模拟底层主动推送
    std::thread inputThread([&webview]() {
        std::string line;
        Shin::LOGI("Console") << "Enter command in format: <id> <content> (e.g. '1 Hello C++!')";
        
        while (std::getline(std::cin, line)) {
            size_t spacePos = line.find(' ');
            if (spacePos != std::string::npos) {
                try {
                    int id = std::stoi(line.substr(0, spacePos));
                    std::string content = line.substr(spacePos + 1);
                    
                    void* ptr = webview.GetSharedMemory(id);
                    if (ptr) {
                        size_t size = webview.GetSharedMemorySize(id);
                        // 零拷贝写入内存 (保证不越界)
                        size_t copyLen = (std::min)(content.length() + 1, size);
                        memcpy(ptr, content.c_str(), copyLen);
                        static_cast<char*>(ptr)[copyLen - 1] = '\0'; // 确保以 \0 结尾
                        
                        // 主动派发更新通知给 JS
                        Shin::UI::WebviewMessageHandler::NotifySharedMemoryUpdate(id);
                    } else {
                        Shin::LOGE("Console") << "Shared memory ID " << id << " does not exist.";
                    }
                } catch (...) {
                    Shin::LOGE("Console") << "Invalid format. Use: <id> <content>";
                }
            } else {
                 Shin::LOGE("Console") << "Invalid format. Use: <id> <content>";
            }
        }
    });
    inputThread.detach();

    Shin::LOGI("Main") << "Starting UI message loop (blocking)...";
    webview.RunBlocking();
    Shin::LOGI("Main") << "UI loop exited successfully.";

    return 0;
}
