#include "Config.hpp"
#include <Log.hpp>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    Shin::Log::Init();
    
    // 1. Setup defaults before first GetInstance()
    std::unordered_map<std::string, std::string> defaults = {
        {"server_ip", "127.0.0.1"},
        {"port", "9000"},
        {"app_name", "Shin-Test"}
    };
    
    // We can't call SetDefaults on Instance if it auto-loads, 
    // but we can call it right after or change GetInstance to not Load if we want.
    // Given the requirement "获取单例类的时候，顺便调用Load", 
    // we should ideally have a way to set defaults early.
    
    auto& config = Shin::Core::Config::GetInstance();
    config.SetDefaults(defaults); // Merge them now

    Shin::LOGI("ConfigTest") << "Config loaded. App Name: " << config.GetValue("app_name");

    // 2. Modify value
    config.SetValue("last_run", "2026-06-01");
    config.SetValue("port", "9001"); // Overriding default

    Shin::LOGI("ConfigTest") << "Modifyed port to: " << config.GetValue("port");
    Shin::LOGI("ConfigTest") << "Check the file 'manifest.toml' in your build directory.";
    Shin::LOGI("ConfigTest") << "It should be locked now. Try to edit it with another app.";

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::seconds(2));

    Shin::LOGI("ConfigTest") << "Exiting... changes should be saved automatically.";
    return 0;
}
