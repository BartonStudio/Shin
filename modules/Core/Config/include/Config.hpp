#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <toml.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include "IConfigurable.hpp"
#include <Log.hpp>

namespace Shin {
    namespace Core {

        class SHIN_API Config {
        public:
            /**
             * @brief Get the singleton instance. 
             * @param path Optional path for initialization. Only effective on the first call.
             *             If empty on first call, defaults to manifest.toml in executable directory.
             */
            static Config& GetInstance(const std::filesystem::path& path = "");

            /**
             * @brief Set hardcoded default values. Should be called before the first GetInstance() 
             * or during early initialization to be effective.
             */
            void SetDefaults(const std::unordered_map<std::string, std::string>& defaults);

            /**
             * @brief Load configuration. 
             * @param path If empty, defaults to manifest.toml in executable directory.
             * @return true if successful.
             */
            bool Load(const std::filesystem::path& path = "");

            /**
             * @brief Get a value by key.
             */
            std::string GetValue(const std::string& key, const std::string& defaultValue = "");

            /**
             * @brief Set a value in memory. Will be persisted on exit.
             */
            void SetValue(const std::string& key, const std::string& value);

            /**
             * @brief Access the raw toml object if needed.
             */
            toml::value& GetData() { return m_data; }

            /**
             * @brief Explicitly save current configuration to disk.
             * Should be called during application shutdown.
             */
            void Save();

            /**
             * @brief Clear all data from memory. 
             * Useful to ensure toml objects are destroyed before DLLs are unloaded.
             */
            void Clear();

            /**
             * @brief Register a configurable module.
             */
            void Register(IConfigurable* configurable);

            /**
             * @brief Unregister a configurable module.
             */
            void Unregister(IConfigurable* configurable);

        private:
            Config() = default;
            ~Config(); // Handles final save and file closure
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;

            bool InternalSave();
            std::filesystem::path GetDefaultManifestPath();
            
            // 辅助函数：解析 a.b.c 路径并返回对应的 toml 节点指针
            toml::value* NavigateTo(toml::value& root, const std::string& path, bool createMissing);

            toml::value m_data;
            std::unordered_map<std::string, std::string> m_defaults;
            std::filesystem::path m_activePath;
            std::fstream m_fileHandle; 
            
            std::vector<IConfigurable*> m_configurables;
            std::mutex m_configurablesMutex;
            
            static std::once_flag s_initFlag;
        };

    } // namespace Core
} // namespace Shin
