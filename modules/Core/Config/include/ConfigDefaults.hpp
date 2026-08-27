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
                {"Webview.CachePath", "AppCache"},

                // 调试菜单开关
                {"Webview.EnableDevMenu", "false"},

                // Browser extension switch (extension paths are module-owned configuration).
                {"Webview.BrowserExtensionsEnabled", "false"},

                // 远程调试端口（0 = 关闭；>0 = 在 127.0.0.1:<port> 开启 CDP 远程调试）
                {"Webview.RemoteDebuggingPort", "0"},

                // 日历数据库路径
                {"Calendar.db_path", "memos.db"}
            };

        } // namespace Defaults
    } // namespace Core
} // namespace Shin
