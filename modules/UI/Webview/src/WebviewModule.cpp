#include <WebviewWrapper.hpp>
#include <Module.hpp>
#include <Log.hpp>
#include <Config.hpp>
#include <atomic>
#include <iostream>

// Win32 headers for window style modification
#ifdef _WIN32
#include <windows.h>
#endif

namespace Shin {
namespace UI {

    static std::atomic<bool> s_isShuttingDown{false};

    /**
     * @brief Webview Module Implementation.
     */
    class WebviewModule : public IModule {
    public:
        std::string GetModuleName() const override { return "Webview"; }

        bool OnInitialize(Core::Config& config) override {
            LOGI("Webview") << "Initializing Webview Module...";
            s_isShuttingDown = false;

            auto& webview = WebviewWrapper::GetInstance();

            // 1. Load configuration
            std::string appName = config.GetValue("app_name", "Shin Application");
            int width = std::stoi(config.GetValue("window_width", "1280"));
            int height = std::stoi(config.GetValue("window_height", "720"));
            bool isFrameless = config.GetValue("frameless", "true") == "true";

            // 2. Configure Wrapper
            webview.SetTitle(appName);
            webview.SetSize(width, height, false);
            webview.SetDebug(true);

            // 3. Initialize Native Webview
            if (!webview.Initialize()) {
                LOGE("Webview") << "Failed to initialize native webview.";
                return false;
            }

            webview.OpenDevTools();

            // Apply frameless style if needed
            if (isFrameless) {
                HWND hwnd = (HWND)webview.GetNativeWindow();
                if (hwnd) {
                    LONG style = GetWindowLong(hwnd, GWL_STYLE);
                    // Remove WS_CAPTION and WS_THICKFRAME to remove border/title bar/resizing
                    style &= ~(WS_CAPTION | WS_THICKFRAME);
                    SetWindowLong(hwnd, GWL_STYLE, style);
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
                    LOGI("Webview") << "Applied simple frameless style.";
                }
            }

            // 4. Navigate to Startup URL
            std::string startupUrl = config.GetValue("startup_url", "http://localhost:8080");
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
