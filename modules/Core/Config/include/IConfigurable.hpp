#pragma once
#include <string>
#include <toml.hpp>

namespace Shin {
    namespace Core {

        /**
         * @brief Interface for modules that want to be automatically configured.
         */
        class IConfigurable {
        public:
            virtual ~IConfigurable() = default;

            /**
             * @brief Returns the dot-separated path in the TOML tree this module cares about.
             * e.g., "system.auth" or "ui.webview"
             */
            virtual std::string GetConfigPath() const = 0;

            /**
             * @brief Called by the Config module after a file is loaded or when defaults are applied.
             * @param data The toml::value corresponding to the path returned by GetConfigPath().
             */
            virtual void OnConfigLoad(const toml::value& data) = 0;

            /**
             * @brief Called by the Config module before saving to allow the module to update the TOML data.
             * @param data The toml::value to be modified.
             */
            virtual void OnConfigSave(toml::value& data) const = 0;
        };

    } // namespace Core
} // namespace Shin
