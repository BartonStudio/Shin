#include "Config.hpp"
#include "ConfigDefaults.hpp"
#include <Log.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Shin {
    namespace Core {

        std::once_flag Config::s_initFlag;

        Config& Config::GetInstance(const std::filesystem::path& path) {
            static Config instance;
            std::call_once(s_initFlag, [&]() {
                instance.Load(path); 
            });
            return instance;
        }

        Config::~Config() {
            // DO NOT call InternalSave here. 
            // Static destruction order is unpredictable and modules may already be dead.
            if (m_fileHandle.is_open()) {
                m_fileHandle.close();
            }
        }

        void Config::Save() {
            InternalSave();
        }

        void Config::Clear() {
            std::lock_guard<std::mutex> lock(m_configurablesMutex);
            m_data = toml::value(toml::table{});
            m_configurables.clear();
        }

        void Config::Register(IConfigurable* configurable) {
            if (!configurable) return;
            
            std::lock_guard<std::mutex> lock(m_configurablesMutex);
            m_configurables.push_back(configurable);
            
            // If we already have data, notify the newly registered module immediately
            if (!m_data.is_empty()) {
                toml::value* node = NavigateTo(m_data, configurable->GetConfigPath(), false);
                if (node) {
                    configurable->OnConfigLoad(*node);
                } else {
                    // Provide an empty table if the node doesn't exist yet
                    toml::value empty_table = toml::table{};
                    configurable->OnConfigLoad(empty_table);
                }
            }
        }

        void Config::Unregister(IConfigurable* configurable) {
            std::lock_guard<std::mutex> lock(m_configurablesMutex);
            m_configurables.erase(
                std::remove(m_configurables.begin(), m_configurables.end(), configurable),
                m_configurables.end()
            );
        }

        void Config::SetDefaults(const std::unordered_map<std::string, std::string>& defaults) {
            m_defaults = defaults;
            for (const auto& [key, val] : m_defaults) {
                if (!NavigateTo(m_data, key, false)) {
                    SetValue(key, val);
                }
            }
        }

        bool Config::Load(const std::filesystem::path& path) {
            m_activePath = path.empty() ? GetDefaultManifestPath() : path;

            try {
                if (!std::filesystem::exists(m_activePath)) {
                    std::ofstream create_file(m_activePath);
                    create_file << "# Shin Manifest Configuration\n";
                    create_file.close();
                }

                if (!m_fileHandle.is_open()) {
                    m_fileHandle.open(m_activePath, std::ios::in | std::ios::out);
                }

                if (!m_fileHandle.is_open()) {
                    LOGE("Config") << "Could not open/lock config file: " << m_activePath.string();
                    return false;
                }

                m_data = toml::parse(m_activePath.string());

                // Merge hardcoded defaults
                for (const auto& [key, val] : Defaults::Manifest) {
                    if (!NavigateTo(m_data, key, false)) {
                        SetValue(key, val);
                    }
                }

                // Merge runtime defaults
                for (const auto& [key, val] : m_defaults) {
                    if (!NavigateTo(m_data, key, false)) {
                        SetValue(key, val);
                    }
                }

                // Dispatch to all registered modules
                {
                    std::lock_guard<std::mutex> lock(m_configurablesMutex);
                    for (auto* configurable : m_configurables) {
                        toml::value* node = NavigateTo(m_data, configurable->GetConfigPath(), false);
                        if (node) {
                            configurable->OnConfigLoad(*node);
                        } else {
                            toml::value empty_table = toml::table{};
                            configurable->OnConfigLoad(empty_table);
                        }
                    }
                }

                LOGI("Config") << "Config initialized and dispatched: " << m_activePath.string();
                return true;

            } catch (const std::exception& e) {
                LOGE("Config") << "Load Error: " << e.what();
                return false;
            }
        }

        std::string Config::GetValue(const std::string& key, const std::string& defaultValue) {
            toml::value* node = NavigateTo(m_data, key, false);
            if (node) {
                try {
                    if (node->is_string()) return toml::get<std::string>(*node);
                    if (node->is_integer()) return std::to_string(toml::get<int64_t>(*node));
                    if (node->is_floating()) return std::to_string(toml::get<double>(*node));
                    if (node->is_boolean()) return toml::get<bool>(*node) ? "true" : "false";
                    
                    std::stringstream ss;
                    ss << *node;
                    return ss.str();
                } catch (...) {
                    return defaultValue;
                }
            }
            return defaultValue;
        }

        void Config::SetValue(const std::string& key, const std::string& value) {
            toml::value* node = NavigateTo(m_data, key, true);
            if (node) {
                *node = value;
            }
        }

        toml::value* Config::NavigateTo(toml::value& root, const std::string& path, bool createMissing) {
            std::vector<std::string> keys;
            std::string segment;
            std::stringstream ss(path);
            while (std::getline(ss, segment, '.')) {
                keys.push_back(segment);
            }

            toml::value* current = &root;
            for (size_t i = 0; i < keys.size(); ++i) {
                const auto& k = keys[i];
                
                if (!current->is_table()) {
                    if (createMissing) {
                        *current = toml::table{};
                    } else {
                        return nullptr;
                    }
                }

                if (!current->contains(k)) {
                    if (createMissing) {
                        // 始终创建一个 table 作为中间层或末端，除非明确需要标量
                        (*current)[k] = toml::table{};
                    } else {
                        return nullptr;
                    }
                }
                current = &((*current)[k]);
            }
            return current;
        }

        bool Config::InternalSave() {
            // Pull latest data from modules before saving
            {
                std::lock_guard<std::mutex> lock(m_configurablesMutex);
                for (auto* configurable : m_configurables) {
                    toml::value* node = NavigateTo(m_data, configurable->GetConfigPath(), true);
                    if (node) {
                        configurable->OnConfigSave(*node);
                    }
                }
            }

            if (!m_fileHandle.is_open()) {
                m_fileHandle.open(m_activePath, std::ios::in | std::ios::out | std::ios::trunc);
            }
            if (!m_fileHandle.is_open()) return false;

            try {
                m_fileHandle.clear();
                m_fileHandle.seekp(0, std::ios::beg);
                
                std::string output = toml::format(m_data);
                m_fileHandle << output;
                m_fileHandle.flush();
                
                LOGI("Config") << "Auto-saved config with module updates to: " << m_activePath.string();
                return true;
            } catch (const std::exception& e) {
                LOGE("Config") << "Save Error: " << e.what();
                return false;
            }
        }

        std::filesystem::path Config::GetDefaultManifestPath() {
#ifdef _WIN32
            char path[MAX_PATH];
            GetModuleFileNameA(NULL, path, MAX_PATH);
            return std::filesystem::path(path).parent_path() / "manifest.toml";
#else
            return std::filesystem::current_path() / "manifest.toml";
#endif
        }

    } // namespace Core
} // namespace Shin
