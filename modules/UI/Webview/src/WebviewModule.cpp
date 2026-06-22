#include <WebviewWrapper.hpp>
#include <WebviewMessageHandler.hpp>
#include <Module.hpp>
#include <Log.hpp>
#include <Config.hpp>
#include <IConfigurable.hpp>
#include <atomic>
#include <iostream>
#include <windows.h>

namespace Shin {
namespace UI {

    static std::atomic<bool> s_isShuttingDown{false};

    /**
     * @brief Webview Module Implementation.
     */
    class WebviewModule : public IModule, public Core::IConfigurable {
    public:
        std::string GetModuleName() const override { return "Webview"; }

        std::string GetConfigPath() const override { return "Webview"; }

        void OnConfigLoad(const toml::value& data) override {
            std::string cacheDir = toml::find_or<std::string>(data, "CachePath", "AppCache");
            // 确保读取时处理字符串配置
            std::string devMenuStr = toml::find_or<std::string>(data, "EnableDevMenu", "false");
            bool enableDevMenu = (devMenuStr == "true");
            
            // 获取可执行文件目录并设置环境变量
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring strExePath(exePath);
            size_t pos = strExePath.find_last_of(L"\\/");
            std::wstring exeDir = strExePath.substr(0, pos);
            std::wstring cachePath = exeDir + L"\\" + std::wstring(cacheDir.begin(), cacheDir.end());
            
            SetEnvironmentVariableW(L"WEBVIEW2_USER_DATA_FOLDER", cachePath.c_str());
            LOGI("Webview") << "WebView2 cache path set to: " << cacheDir;

            // 仅当启用时读取并注入 DevMenu.js
            if (enableDevMenu) {
                std::wstring jsPath = exeDir + L"\\DevMenu.js";
                std::ifstream jsFile(jsPath);
                if (jsFile.is_open()) {
                    std::stringstream buffer;
                    buffer << jsFile.rdbuf();
                    WebviewWrapper::GetInstance().InjectJSBeforeLoad(buffer.str());
                    LOGI("Webview") << "DevMenu.js injected successfully.";
                } else {
                    LOGW("Webview") << "DevMenu.js not found at: " << std::string(jsPath.begin(), jsPath.end());
                }
            }
        }

        void OnConfigSave(toml::value& data) const override {
            // 确保 data 是 table 类型
            if (!data.is_table()) {
                data = toml::table{};
            }
            if (!data.contains("CachePath")) {
                data["CachePath"] = "AppCache";
            }
            if (!data.contains("EnableDevMenu")) {
                data["EnableDevMenu"] = "false";
            }
        }

        bool OnInitialize(Core::Config& config) override {
            // Register with Config to receive updates
            config.Register(this);

            LOGI("Webview") << "Initializing Webview Module...";
            s_isShuttingDown = false;

            auto& webview = WebviewWrapper::GetInstance();

            // 1. Load configuration
            std::string appName = config.GetValue("Webview.app_name", "Shin Application");
            int width = std::stoi(config.GetValue("Webview.window_width", "1280"));
            int height = std::stoi(config.GetValue("Webview.window_height", "720"));
            bool isFrameless = config.GetValue("Webview.frameless", "true") == "true";
            
            // 注册开发工具菜单相关 Action (复用内置的 WindowClose, WindowOpenDevTools)
            UI::WebviewMessageHandler::RegisterAction("ZoomIn", [](const nlohmann::json&, nlohmann::json, auto) {
                WebviewWrapper::GetInstance().ExecuteJS("document.body.style.zoom = (parseFloat(getComputedStyle(document.body).zoom) || 1) + 0.1;");
            });
            UI::WebviewMessageHandler::RegisterAction("ZoomOut", [](const nlohmann::json&, nlohmann::json, auto) {
                WebviewWrapper::GetInstance().ExecuteJS("document.body.style.zoom = (parseFloat(getComputedStyle(document.body).zoom) || 1) - 0.1;");
            });

            // 2. Configure Wrapper
            webview.SetTitle(appName);
            webview.SetSize(width, height, false);
            webview.SetDebug(true);

            // 3. Initialize Native Webview
            if (!webview.Initialize()) {
                LOGE("Webview") << "Failed to initialize native webview.";
                return false;
            }

            // webview.OpenDevTools();

            // Apply frameless style if needed
            if (isFrameless) {
                HWND hwnd = (HWND)webview.GetNativeWindow();
                if (hwnd) {
                    LONG style = GetWindowLong(hwnd, GWL_STYLE);
                    style &= ~(WS_CAPTION | WS_THICKFRAME);
                    SetWindowLong(hwnd, GWL_STYLE, style);
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
                }
            }

            // 4. Navigate to Startup URL
            std::string startupUrl = config.GetValue("Webview.startup_url", "http://localhost:8080");
            webview.Navigate(startupUrl);
            LOGI("Webview") << "Navigating to: " << startupUrl;

            // 5. Inject Log Listener BEFORE page load
            webview.InjectJSBeforeLoad(
                "if (window.chrome && window.chrome.webview) {"
                "  window.chrome.webview.addEventListener('message', function(e) {"
                "    if (typeof e.data === 'object' && e.data.type === 'log') {"
                "      const entry = e.data;"
                "      const styles = {"
                "        'INFO': 'color: #2ecc71', 'WARN': 'color: #f1c40f', 'ERROR': 'color: #e74c3c', 'DEBUG': 'color: #3498db', 'TRACE': 'color: #95a5a6'"
                "      };"
                "      console.log('%c[' + entry.level + '] [' + entry.tag + '] ' + entry.message, styles[entry.level] || '');"
                "    }"
                "  });"
                "}"
            );

            // 6. Set up Log forwarding from C++ to JS
            Log::AddJsonCallbackSink([&webview](const std::string& jsonMsg, const Log::LogEntry&) {
                if (!s_isShuttingDown) {
                    webview.SendJson(jsonMsg);
                }
            });

            return true;
        }

        void OnShutdown() override {
            s_isShuttingDown = true;
            LOGI("Webview") << "Shutting down Webview Module...";
            WebviewWrapper::GetInstance().Terminate();
        }
    };

    SHIN_REGISTER_MODULE(WebviewModule)

} // namespace UI
} // namespace Shin
