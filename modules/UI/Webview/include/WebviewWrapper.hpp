#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <ISharedMemoryManager.hpp>

#ifdef _WIN32
    #ifdef SHIN_UIWEBVIEW_EXPORTS
        #define SHIN_UIWEBVIEW_API __declspec(dllexport)
    #else
        #define SHIN_UIWEBVIEW_API __declspec(dllimport)
    #endif
#else
    #define SHIN_UIWEBVIEW_API __attribute__((visibility("default")))
#endif

// Disable C4251 for std::unique_ptr crossing DLL boundary
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace Shin {
namespace UI {

    class SHIN_UIWEBVIEW_API WebviewWrapper : public Core::ISharedMemoryManager {
    public:
        static WebviewWrapper& GetInstance();

        WebviewWrapper(const WebviewWrapper&) = delete;
        WebviewWrapper& operator=(const WebviewWrapper&) = delete;

        void SetDebug(bool enable);
        // Enables remote debugging (CDP) on localhost:<port>. Must be called before Initialize().
        // port <= 0 disables remote debugging.
        void SetRemoteDebuggingPort(int port);
        void SetParentWindow(void* hwnd);
        
        void SetStartupURL(const std::string& url);
        void SetStartupHTML(const std::string& html);

        void SetContextMenuEnabled(bool enable);

        // Business Layer callback
        void SetJavascriptMessageCallback(std::function<std::string(const std::string&)> callback);

        void InjectJSBeforeLoad(const std::string& js);
        void BindFunction(const std::string& name, std::function<std::string(const std::string&)> fn);

        void SetTitle(const std::string& title);
        void SetSize(int width, int height, bool fixed = false);

        struct BrowserExtensionResult {
            bool success = false;
            std::filesystem::path path;
            std::string name;
            std::int32_t hresult = 0;
            std::string error;
        };
        using BrowserExtensionCallback = std::function<void(const BrowserExtensionResult&)>;

        // Must be configured before Initialize because it changes WebView2 EnvironmentOptions.
        void SetBrowserExtensionsEnabled(bool enable);
        // Installs a local unpacked extension directory. The callback is invoked asynchronously.
        bool AddBrowserExtension(const std::filesystem::path& extensionPath,
                                 BrowserExtensionCallback callback = {});

        bool Initialize();

        void OpenDevTools();

        void* GetNativeWindow();
        void* GetNativeController();

        void RunBlocking();
        void Terminate();
        void Navigate(const std::string& url);
        void ExecuteJS(const std::string& js); 
        
        // ISharedMemoryManager implementation
        void* CreateSharedMemory(int& outId, size_t size) override;
        bool DestroySharedMemory(int id) override;
        void* GetSharedMemory(int id) const override;
        size_t GetSharedMemorySize(int id) const override;
        void SendJson(const std::string& json) override;
        
        // In JS, listen via: window.chrome.webview.addEventListener('message', e => { if(typeof e.data === 'string') ... })
        void SendString(const std::string& str);
        
        // Shared Memory (Persistent & Zero-Copy)
        bool ResizeSharedMemory(int id, size_t newSize);
        bool PostSharedMemoryToWeb(int id, const std::string& additionalData = "{}");

    private:
        WebviewWrapper();
        ~WebviewWrapper();

        struct Impl;
        std::unique_ptr<Impl> m_impl; 
    };

}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
