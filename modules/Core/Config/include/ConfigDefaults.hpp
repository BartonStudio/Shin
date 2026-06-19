#pragma once
#include <unordered_map>
#include <string>

namespace Shin {
    namespace Core {
        namespace Defaults {

            /**
             * @brief 这里存放硬编码的默认配置表。
             * 你可以随时在这里添加新的键值对。
             */
            inline const std::unordered_map<std::string, std::string> Manifest = {
                // 应用基础信息
                {"Webview.app_name", "Shin Application"},

                // 浏览器窗口尺寸
                {"Webview.window_width", "960"},
                {"Webview.window_height", "600"},

                // 启动 URL
                {"Webview.startup_url", "http://localhost:8080"},

                // 窗口样式
                {"Webview.frameless", "true"},

                // 缓存目录
                {"Webview.CachePath", "AppCache"}
            };

        } // namespace Defaults
    } // namespace Core
} // namespace Shin
