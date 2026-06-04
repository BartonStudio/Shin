#pragma once
#include <Log.hpp>
#include <Config.hpp>
#include <Module.hpp>

namespace Shin {

    /**
     * @brief The unified entry point for initializing the Shin Engine and all its modules.
     */
    inline bool Init() {
        // 1. Initialise Log system first so other modules can log during initialization
        Log::Init();
        LOGI("Core") << "Initializing Shin Engine...";

        // 2. Get Config instance (this will load manifest.toml)
        auto& config = Core::Config::GetInstance();

        // 3. Initialize all registered modules
        if (!ModuleManager::GetInstance().InitializeAll(config)) {
            LOGE("Core") << "Failed to initialize one or more modules.";
            return false;
        }

        LOGI("Core") << "Shin Engine initialized successfully.";
        return true;
    }

    inline void Shutdown() {
        LOGI("Core") << "Shutting down Shin Engine...";
        
        auto& config = Core::Config::GetInstance();
        config.Save();
        config.Clear(); // Explicitly destroy toml data

        ModuleManager::GetInstance().ShutdownAll();
    }

} // namespace Shin
