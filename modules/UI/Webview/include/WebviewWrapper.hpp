#pragma once
#include <string>
#include <functional>
#include <memory>
#include <vector>

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

    class SHIN_UIWEBVIEW_API WebviewWrapper {
    public:
        static WebviewWrapper& GetInstance();

        WebviewWrapper(const WebviewWrapper&) = delete;
        WebviewWrapper& operator=(const WebviewWrapper&) = delete;

        void SetDebug(bool enable);
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

        bool Initialize();

        void* GetNativeWindow();
        void* GetNativeController();

        void RunBlocking();
        void Terminate();
        void Navigate(const std::string& url);
        void ExecuteJS(const std::string& js); 
        
        // Native PostMessage
        // In JS, listen via: window.chrome.webview.addEventListener('message', e => { if(typeof e.data === 'object') ... })
        void SendJson(const std::string& json);
        
        // In JS, listen via: window.chrome.webview.addEventListener('message', e => { if(typeof e.data === 'string') ... })
        void SendString(const std::string& str);
        
        // Shared Memory (Persistent & Zero-Copy)
        // 1. C++ creates memory: void* ptr = CreateSharedMemory(0, 1024);
        // 2. C++ posts to JS: PostSharedMemoryToWeb(0, "{\"init\":true}");
        // 3. JS receives memory handle ONCE via:
        //    window.chrome.webview.addEventListener('sharedbufferreceived', e => { const buffer = e.getBuffer(); ... })
        // 4. C++ modifies memory directly via ptr, then calls SendJson to notify JS to read the updated buffer.
        void* CreateSharedMemory(int& outId, size_t size);
        bool ResizeSharedMemory(int id, size_t newSize);
        bool PostSharedMemoryToWeb(int id, const std::string& additionalData = "{}");
        bool DestroySharedMemory(int id);
        void* GetSharedMemory(int id) const;
        size_t GetSharedMemorySize(int id) const;

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