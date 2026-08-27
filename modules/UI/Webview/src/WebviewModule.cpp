#include <WebviewWrapper.hpp>
#include <WebviewMessageHandler.hpp>
#include <Module.hpp>
#include <Log.hpp>
#include <Config.hpp>
#include <IConfigurable.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
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
            std::string extensionsEnabledStr =
                toml::find_or<std::string>(data, "BrowserExtensionsEnabled", "false");
            m_browserExtensionsEnabled = (extensionsEnabledStr == "true");
            m_browserExtensionPaths.clear();
            if (data.contains("BrowserExtensionPaths")) {
                try {
                    m_browserExtensionPaths = toml::get<std::vector<std::string>>(
                        data.at("BrowserExtensionPaths"));
                } catch (const std::exception& exception) {
                    LOGE("Webview") << "BrowserExtensionPaths must be a string array: "
                                     << exception.what();
                }
            }
            
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
            // 仅当启用时读取并注入 ShinDevMenu.js
            if (enableDevMenu) {
                std::wstring jsPath = exeDir + L"\\ShinDevMenu.js";
                std::ifstream jsFile(jsPath);
                if (jsFile.is_open()) {
                    std::stringstream buffer;
                    buffer << jsFile.rdbuf();
                    WebviewWrapper::GetInstance().InjectJSBeforeLoad(buffer.str());
                    LOGI("Webview") << "ShinDevMenu.js injected successfully.";
                } else {
                    LOGW("Webview") << "ShinDevMenu.js not found at: " << std::string(jsPath.begin(), jsPath.end());
                }
            }
        }

    private:
        bool m_browserExtensionsEnabled = false;
        std::vector<std::string> m_browserExtensionPaths;

    public:
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
            if (!data.contains("BrowserExtensionsEnabled")) {
                data["BrowserExtensionsEnabled"] = "false";
            }
            if (!data.contains("BrowserExtensionPaths")) {
                data["BrowserExtensionPaths"] = toml::array{};
            }
            if (!data.contains("RemoteDebuggingPort")) {
                data["RemoteDebuggingPort"] = "0";
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

            LOGI("Webview") << "Window: " << appName << " " << width << "x" << height
                            << " frameless=" << (isFrameless ? "true" : "false");

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
            webview.SetBrowserExtensionsEnabled(m_browserExtensionsEnabled);

            // 远程调试：仅当配置了有效端口（>0）时开启，绑定 127.0.0.1 回环地址
            int remoteDebuggingPort = 0;
            try {
                remoteDebuggingPort =
                    std::stoi(config.GetValue("Webview.RemoteDebuggingPort", "0"));
            } catch (...) {
                remoteDebuggingPort = 0;
            }
            webview.SetRemoteDebuggingPort(remoteDebuggingPort);
            if (remoteDebuggingPort > 0) {
                LOGI("Webview") << "Remote debugging enabled on http://127.0.0.1:"
                                << remoteDebuggingPort;
            }

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

            // 4. Install configured extensions before the first page navigation.
            const std::string startupUrl = config.GetValue("Webview.startup_url", "http://localhost:8080");
            auto navigateStartup = [startupUrl, &webview]() {
                webview.Navigate(startupUrl);
                LOGI("Webview") << "Navigating to: " << startupUrl;
            };

            if (!m_browserExtensionsEnabled || m_browserExtensionPaths.empty()) {
                navigateStartup();
            } else {
                wchar_t executablePath[MAX_PATH]{};
                GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
                const std::filesystem::path executableDirectory =
                    std::filesystem::path(executablePath).parent_path();
                auto pendingInstallations = std::make_shared<std::atomic<size_t>>(0);

                for (const auto& configuredPath : m_browserExtensionPaths) {
                    std::filesystem::path extensionPath(configuredPath);
                    if (extensionPath.is_relative()) {
                        extensionPath = executableDirectory / extensionPath;
                    }
                    ++(*pendingInstallations);
                    const bool submitted = webview.AddBrowserExtension(
                        extensionPath,
                        [extensionPath, pendingInstallations, navigateStartup](
                            const WebviewWrapper::BrowserExtensionResult& result) {
                            if (result.success) {
                                LOGI("Webview") << "Browser extension installed: "
                                                 << extensionPath.string()
                                                 << (result.name.empty() ? "" : " (" + result.name + ")");
                            } else {
                                LOGE("Webview") << "Browser extension failed: "
                                                 << extensionPath.string()
                                                 << ", HRESULT=" << result.hresult
                                                 << ", " << result.error;
                            }
                            if (--(*pendingInstallations) == 0) {
                                navigateStartup();
                            }
                        });
                    if (!submitted) {
                        LOGE("Webview") << "Browser extension installation was not submitted: "
                                         << extensionPath.string();
                    }
                }
            }

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
