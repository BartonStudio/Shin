#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <Config.hpp>

namespace Shin {

    /**
     * @brief Base interface for all system modules.
     */
    class SHIN_API IModule {
    public:
        virtual ~IModule() = default;
        virtual std::string GetModuleName() const = 0;
        
        /**
         * @brief Called during Shin::Init(). 
         * @param config Reference to the global configuration system.
         * @return true if initialization was successful.
         */
        virtual bool OnInitialize(Core::Config& config) = 0;
        
        /**
         * @brief Called before application exit.
         */
        virtual void OnShutdown() = 0;
    };

    /**
     * @brief Manages the lifecycle of all registered modules.
     */
    class SHIN_API ModuleManager {
    public:
        static ModuleManager& GetInstance();

        void RegisterModule(std::unique_ptr<IModule> module) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_modules.push_back(std::move(module));
        }

        bool InitializeAll(Core::Config& config) {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& module : m_modules) {
                if (!module->OnInitialize(config)) {
                    return false;
                }
            }
            return true;
        }

        void ShutdownAll() {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Shutdown in reverse order of registration
            for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
                (*it)->OnShutdown();
            }
            m_modules.clear();
        }

    private:
        ModuleManager() = default;
        std::vector<std::unique_ptr<IModule>> m_modules;
        std::mutex m_mutex;
    };

    /**
     * @brief Helper class to register modules automatically via static initialization.
     */
    template<typename T>
    class ModuleRegistrar {
    public:
        ModuleRegistrar() {
            ModuleManager::GetInstance().RegisterModule(std::make_unique<T>());
        }
    };

    #define SHIN_REGISTER_MODULE(ClassName) \
        static Shin::ModuleRegistrar<ClassName> g_registrar_##ClassName;

} // namespace Shin
