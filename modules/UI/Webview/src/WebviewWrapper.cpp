#include "WebviewWrapper.hpp"
#include "WebviewMessageHandler.hpp"
#include <Log.hpp>
#include <webview.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>
#include <filesystem>

#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <WebView2.h>
#include <wrl.h>
#pragma comment(lib, "comctl32.lib")
#endif

// 声明内部链接的 ProcessMessage
namespace Shin {
namespace UI {
namespace WebviewMessageHandler {
    std::string ProcessMessage(const std::string& jsonRequest);
}
}
}

#ifdef _WIN32
namespace {
    // 转发声明
    LRESULT CALLBACK HostSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    LRESULT CALLBACK ChildSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam);

    LRESULT CALLBACK HostSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        switch (uMsg) {
        case WM_NCCALCSIZE: {
            if (wParam) {
                // 如果是无边框模式，我们告诉系统客户区覆盖整个窗口
                DWORD style = GetWindowLongPtr(hWnd, GWL_STYLE);
                if (!(style & WS_CAPTION)) {
                    if (IsZoomed(hWnd)) {
                        // 最大化时需要留出边距，否则内容会超出屏幕或覆盖任务栏
                        LPNCCALCSIZE_PARAMS pncsp = reinterpret_cast<LPNCCALCSIZE_PARAMS>(lParam);
                        int border = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                        pncsp->rgrc[0].top += border;
                        pncsp->rgrc[0].left += border;
                        pncsp->rgrc[0].right -= border;
                        pncsp->rgrc[0].bottom -= border;
                    }
                    return 0;
                }
            }
            break;
        }
        case WM_NCHITTEST: {
            // 先尝试系统的默认逻辑（这会处理滚动条等）
            LRESULT hit = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            
            DWORD style = GetWindowLongPtr(hWnd, GWL_STYLE);
            // 只有无边框且非最大化时，才执行自定义边缘探测
            if (!(style & WS_CAPTION) && (style & WS_THICKFRAME) && !IsZoomed(hWnd)) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT rc;
                GetWindowRect(hWnd, &rc);
                
                const int span = 10; // 足够大的探测范围

                bool left = (pt.x < rc.left + span);
                bool right = (pt.x >= rc.right - span);
                bool top = (pt.y < rc.top + span);
                bool bottom = (pt.y >= rc.bottom - span);

                if (top && left) return HTTOPLEFT;
                if (top && right) return HTTOPRIGHT;
                if (bottom && left) return HTBOTTOMLEFT;
                if (bottom && right) return HTBOTTOMRIGHT;
                if (top) return HTTOP;
                if (bottom) return HTBOTTOM;
                if (left) return HTLEFT;
                if (right) return HTRIGHT;
            }
            return hit;
        }
        case WM_SIZE:
        case WM_SHOWWINDOW: {
            EnumChildWindows(hWnd, EnumChildProc, (LPARAM)hWnd);
            break;
        }
        case WM_GETMINMAXINFO: {
            // 确保最大化时不会遮挡任务栏
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            if (monitor) {
                MONITORINFO mi = { sizeof(mi) };
                if (GetMonitorInfo(monitor, &mi)) {
                    mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                    mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                    mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                    mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
                }
            }
            return 0;
        }
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    LRESULT CALLBACK ChildSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        if (uMsg == WM_NCHITTEST) {
            HWND hostHwnd = (HWND)dwRefData;
            DWORD style = GetWindowLongPtr(hostHwnd, GWL_STYLE);

            if (!(style & WS_CAPTION) && (style & WS_THICKFRAME) && !IsZoomed(hostHwnd)) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT rc;
                GetWindowRect(hostHwnd, &rc);
                
                const int span = 10;
                // 检查鼠标是否在宿主窗口的边缘
                if (pt.x < rc.left + span || pt.x >= rc.right - span ||
                    pt.y < rc.top + span || pt.y >= rc.bottom - span) {
                    // 让消息穿透到宿主窗口
                    return HTTRANSPARENT;
                }
            }
        } else if (uMsg == WM_NCDESTROY) {
            RemoveWindowSubclass(hWnd, ChildSubclassProc, uIdSubclass);
            RemovePropW(hWnd, L"ShinSubclassed");
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam) {
        // 检查是否已经子类化过
        if (!GetPropW(hWnd, L"ShinSubclassed")) {
            SetWindowSubclass(hWnd, ChildSubclassProc, 2, lParam);
            SetPropW(hWnd, L"ShinSubclassed", (HANDLE)1);
            
            // 继续向下递归枚举子窗口的子窗口
            EnumChildWindows(hWnd, EnumChildProc, lParam);
        }
        return TRUE;
    }
}
#endif

namespace Shin {
namespace UI {

    struct WebviewWrapper::Impl {
        bool debug = false;
        void* parentWindow = nullptr;
        std::string startupUrl;
        std::string startupHtml;
        bool contextMenuEnabled = false;
        std::string title = "Shin UI";
        int width = 800;
        int height = 600;
        int hints = WEBVIEW_HINT_NONE;
        
        struct BindData {
            std::string name;
            std::function<std::string(const std::string&)> fn;
        };
        std::vector<BindData> binds;
        std::vector<std::string> initScripts;
        std::function<std::string(const std::string&)> jsCallback;

        std::unique_ptr<webview::webview> w;
        std::atomic<bool> isInitialized{false};
        std::thread::id uiThreadId;

#ifdef _WIN32
        struct SharedMemoryData {
            Microsoft::WRL::ComPtr<ICoreWebView2SharedBuffer> sharedBuffer;
            void* mappedSharedMemory = nullptr;
            size_t sharedMemorySize = 0;
        };
        std::unordered_map<int, SharedMemoryData> sharedMemories;
        int nextSharedMemoryId = 1;
#endif
    };

    WebviewWrapper& WebviewWrapper::GetInstance() {
        static WebviewWrapper instance;
        return instance;
    }

    WebviewWrapper::WebviewWrapper() : m_impl(std::make_unique<Impl>()) {}
    WebviewWrapper::~WebviewWrapper() {
#ifdef _WIN32
        if (m_impl) {
            for (auto& pair : m_impl->sharedMemories) {
                if (pair.second.mappedSharedMemory) {
                    UnmapViewOfFile(pair.second.mappedSharedMemory);
                }
            }
        }
#endif
    }

    void WebviewWrapper::SetDebug(bool enable) {
        if (!m_impl->isInitialized) m_impl->debug = enable;
    }

    void WebviewWrapper::SetParentWindow(void* hwnd) {
        if (!m_impl->isInitialized) m_impl->parentWindow = hwnd;
    }

    void WebviewWrapper::SetStartupURL(const std::string& url) {
        if (!m_impl->isInitialized) m_impl->startupUrl = url;
    }

    void WebviewWrapper::SetStartupHTML(const std::string& html) {
        if (!m_impl->isInitialized) m_impl->startupHtml = html;
    }

    void WebviewWrapper::SetJavascriptMessageCallback(std::function<std::string(const std::string&)> callback) {
        if (!m_impl->isInitialized) m_impl->jsCallback = callback;
    }

    void WebviewWrapper::SetContextMenuEnabled(bool enable) {
        m_impl->contextMenuEnabled = enable;
        if (m_impl->w) {
#ifdef _WIN32
            auto controller = (ICoreWebView2Controller*)GetNativeController();
            if (controller) {
                Microsoft::WRL::ComPtr<ICoreWebView2> wv2;
                if (SUCCEEDED(controller->get_CoreWebView2(&wv2))) {
                    Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
                    if (SUCCEEDED(wv2->get_Settings(&settings))) {
                        settings->put_AreDefaultContextMenusEnabled(enable);
                    }
                }
            }
#endif
        }
    }

    void WebviewWrapper::InjectJSBeforeLoad(const std::string& js) {
        if (!m_impl->isInitialized) m_impl->initScripts.push_back(js);
    }

    void WebviewWrapper::BindFunction(const std::string& name, std::function<std::string(const std::string&)> fn) {
        if (!m_impl->isInitialized) {
            m_impl->binds.push_back({name, fn});
        }
    }

    void WebviewWrapper::SetTitle(const std::string& title) {
        m_impl->title = title;
        if (m_impl->isInitialized && m_impl->w) {
            if (std::this_thread::get_id() == m_impl->uiThreadId) {
                m_impl->w->set_title(title);
            } else {
                m_impl->w->dispatch([this, title]() { m_impl->w->set_title(title); });
            }
        }
    }

    void WebviewWrapper::SetSize(int width, int height, bool fixed) {
        m_impl->width = width;
        m_impl->height = height;
        m_impl->hints = fixed ? WEBVIEW_HINT_FIXED : WEBVIEW_HINT_NONE;
        
        if (m_impl->isInitialized && m_impl->w) {
            int w_copy = m_impl->width;
            int h_copy = m_impl->height;
            int hints_copy = m_impl->hints;
            if (std::this_thread::get_id() == m_impl->uiThreadId) {
                m_impl->w->set_size(w_copy, h_copy, static_cast<webview_hint_t>(hints_copy));
            } else {
                m_impl->w->dispatch([this, w_copy, h_copy, hints_copy]() { 
                    m_impl->w->set_size(w_copy, h_copy, static_cast<webview_hint_t>(hints_copy)); 
                });
            }
        }
    }

    bool WebviewWrapper::Initialize() {
        if (m_impl->isInitialized) return true;

        try {
            m_impl->w = std::make_unique<webview::webview>(m_impl->debug, m_impl->parentWindow);
            m_impl->uiThreadId = std::this_thread::get_id();

            m_impl->w->set_title(m_impl->title);
            m_impl->w->set_size(m_impl->width, m_impl->height, static_cast<webview_hint_t>(m_impl->hints));

            // Apply Context Menu settings
            SetContextMenuEnabled(m_impl->contextMenuEnabled);

            for (const auto& js : m_impl->initScripts) {
                m_impl->w->init(js);
            }

            // Initialize custom namespace
            m_impl->w->init("window.Shin = window.Shin || {};");

            // Register Fixed Business Layer Entry Point (Auto-mounted to WebviewMessageHandler)
            m_impl->w->bind("__sendDataToCpp__", [this](const std::string& req) -> std::string {
                // If a dynamic callback was set via SetJavascriptMessageCallback, use it.
                if (m_impl->jsCallback) {
                    return m_impl->jsCallback(req);
                }
                // Otherwise, automatically route to the built-in business layer handler.
                return WebviewMessageHandler::ProcessMessage(req);
            });
            m_impl->w->init("window.Shin.sendDataToCpp = window.__sendDataToCpp__; delete window.__sendDataToCpp__;");

            // Keep custom binds working for backward compatibility or special cases
            for (const auto& b : m_impl->binds) {
                m_impl->w->bind(b.name, [fn = b.fn](const std::string& req) -> std::string {
                    return fn(req);
                });

                // Auto-map internal bound functions that start with __ and end with __ 
                // e.g. "__sendDataToCpp__" -> "window.Shin.sendDataToCpp"
                if (b.name.size() > 4 && b.name.substr(0, 2) == "__" && b.name.substr(b.name.size() - 2) == "__") {
                    std::string exposedName = b.name.substr(2, b.name.size() - 4);
                    std::string mappingScript = 
                        "window.Shin." + exposedName + " = window." + b.name + "; "
                        "delete window." + b.name + ";";
                    m_impl->w->init(mappingScript);
                }
            }

            if (!m_impl->startupHtml.empty()) {
                m_impl->w->set_html(m_impl->startupHtml);
            } else if (!m_impl->startupUrl.empty()) {
                m_impl->w->navigate(m_impl->startupUrl);
            }

            m_impl->isInitialized = true;

#ifdef _WIN32
            HWND hostHwnd = (HWND)GetNativeWindow();
            if (hostHwnd) {
                // 1. 宿主窗口子类化：处理边缘碰撞检测
                SetWindowSubclass(hostHwnd, HostSubclassProc, 1, 0);
                
                // 2. 子窗口枚举并子类化：让 WebView2 的边缘消息穿透到宿主
                // 稍微延迟一下确保 WebView2 子窗口已创建（可选，这里先同步尝试）
                EnumChildWindows(hostHwnd, EnumChildProc, (LPARAM)hostHwnd);
            }
#endif

            return true;
        } catch (...) {
            return false;
        }
    }

    void* WebviewWrapper::GetNativeWindow() {
        if (m_impl->w) {
            auto res = m_impl->w->window();
            if (res.ok()) return res.value();
        }
        return nullptr;
    }

    void* WebviewWrapper::GetNativeController() {
        if (m_impl->w) {
            auto res = m_impl->w->browser_controller();
            if (res.ok()) return res.value();
        }
        return nullptr;
    }



    void WebviewWrapper::RunBlocking() {
        if (m_impl->isInitialized && m_impl->w) {
            m_impl->w->run();
        }
    }

    void WebviewWrapper::Terminate() {
        if (m_impl->isInitialized && m_impl->w) {
            m_impl->w->terminate();
        }
    }

    void WebviewWrapper::Navigate(const std::string& url) {
        if (m_impl->isInitialized && m_impl->w) {
            if (std::this_thread::get_id() == m_impl->uiThreadId) {
                m_impl->w->navigate(url);
            } else {
                m_impl->w->dispatch([this, url]() { m_impl->w->navigate(url); });
            }
        }
    }

    void WebviewWrapper::ExecuteJS(const std::string& js) {
        if (m_impl->isInitialized && m_impl->w) {
            if (std::this_thread::get_id() == m_impl->uiThreadId) {
                m_impl->w->eval(js);
            } else {
                m_impl->w->dispatch([this, js]() { m_impl->w->eval(js); });
            }
        }
    }

    void WebviewWrapper::SendJson(const std::string& json) {
        if (!m_impl->isInitialized || !m_impl->w) return;

        if (std::this_thread::get_id() != m_impl->uiThreadId) {
            m_impl->w->dispatch([this, json]() { SendJson(json); });
            return;
        }

#ifdef _WIN32
        auto controller = (ICoreWebView2Controller*)GetNativeController();
        if (controller) {
            Microsoft::WRL::ComPtr<ICoreWebView2> wv2;
            if (SUCCEEDED(controller->get_CoreWebView2(&wv2))) {
                int size_needed = MultiByteToWideChar(CP_UTF8, 0, json.c_str(), (int)json.size(), NULL, 0);
                std::wstring wjson(size_needed, 0);
                MultiByteToWideChar(CP_UTF8, 0, json.c_str(), (int)json.size(), &wjson[0], size_needed);
                wv2->PostWebMessageAsJson(wjson.c_str());
            }
        }
#endif
    }

    void WebviewWrapper::SendString(const std::string& str) {
        if (!m_impl->isInitialized || !m_impl->w) return;

        if (std::this_thread::get_id() != m_impl->uiThreadId) {
            m_impl->w->dispatch([this, str]() { SendString(str); });
            return;
        }

#ifdef _WIN32
        auto controller = (ICoreWebView2Controller*)GetNativeController();
        if (controller) {
            Microsoft::WRL::ComPtr<ICoreWebView2> wv2;
            if (SUCCEEDED(controller->get_CoreWebView2(&wv2))) {
                int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
                std::wstring wstr(size_needed, 0);
                MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
                wv2->PostWebMessageAsString(wstr.c_str());
            }
        }
#endif
    }

    void* WebviewWrapper::CreateSharedMemory(int& outId, size_t size) {
        outId = -1;
        if (!m_impl->isInitialized || !m_impl->w || size == 0) return nullptr;

#ifdef _WIN32
        auto controller = (ICoreWebView2Controller*)GetNativeController();
        if (!controller) return nullptr;

        Microsoft::WRL::ComPtr<ICoreWebView2> wv2;
        if (FAILED(controller->get_CoreWebView2(&wv2))) return nullptr;

        Microsoft::WRL::ComPtr<ICoreWebView2_2> wv2_2;
        if (FAILED(wv2.As(&wv2_2))) return nullptr;

        Microsoft::WRL::ComPtr<ICoreWebView2Environment> env;
        wv2_2->get_Environment(&env);
        if (!env) return nullptr;

        Microsoft::WRL::ComPtr<ICoreWebView2Environment12> env12;
        if (FAILED(env.As(&env12))) return nullptr;

        Impl::SharedMemoryData shm;
        if (FAILED(env12->CreateSharedBuffer(size, &shm.sharedBuffer))) return nullptr;

        HANDLE handle = NULL;
        if (SUCCEEDED(shm.sharedBuffer->get_FileMappingHandle(&handle)) && handle) {
            shm.mappedSharedMemory = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
            shm.sharedMemorySize = size;
            
            int id = m_impl->nextSharedMemoryId++;
            m_impl->sharedMemories[id] = shm;
            outId = id;
            
            return shm.mappedSharedMemory;
        }
#endif
        return nullptr;
    }

    bool WebviewWrapper::ResizeSharedMemory(int id, size_t newSize) {
        if (!m_impl->isInitialized || !m_impl->w || newSize == 0) return false;

#ifdef _WIN32
        auto it = m_impl->sharedMemories.find(id);
        if (it == m_impl->sharedMemories.end()) return false;

        auto controller = (ICoreWebView2Controller*)GetNativeController();
        if (!controller) return false;

        Microsoft::WRL::ComPtr<ICoreWebView2> wv2;
        if (FAILED(controller->get_CoreWebView2(&wv2))) return false;

        Microsoft::WRL::ComPtr<ICoreWebView2_2> wv2_2;
        if (FAILED(wv2.As(&wv2_2))) return false;

        Microsoft::WRL::ComPtr<ICoreWebView2Environment> env;
        wv2_2->get_Environment(&env);
        if (!env) return false;

        Microsoft::WRL::ComPtr<ICoreWebView2Environment12> env12;
        if (FAILED(env.As(&env12))) return false;

        Impl::SharedMemoryData newShm;
        if (FAILED(env12->CreateSharedBuffer(newSize, &newShm.sharedBuffer))) return false;

        HANDLE handle = NULL;
        if (SUCCEEDED(newShm.sharedBuffer->get_FileMappingHandle(&handle)) && handle) {
            newShm.mappedSharedMemory = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, newSize);
            newShm.sharedMemorySize = newSize;
            
            if (it->second.mappedSharedMemory && newShm.mappedSharedMemory) {
                size_t copySize = (std::min)(it->second.sharedMemorySize, newSize);
                memcpy(newShm.mappedSharedMemory, it->second.mappedSharedMemory, copySize);
            }

            if (it->second.mappedSharedMemory) {
                UnmapViewOfFile(it->second.mappedSharedMemory);
            }
            if (it->second.sharedBuffer) {
                it->second.sharedBuffer->Close();
            }

            it->second = newShm;
            return true;
        }
#endif
        return false;
    }

    bool WebviewWrapper::PostSharedMemoryToWeb(int id, const std::string& additionalData) {
        if (!m_impl->isInitialized || !m_impl->w) return false;

        // 核心修复：PostSharedBufferToScript 存在一个深坑！
        // 它的内部实现依赖于底层的异步机制。如果我们把它包装在 dispatch 中推给下一个事件循环，
        // WebView2 会在执行 dispatch lambda 的那一刻发现 COM 对象上下文可能不完全匹配而吞掉事件。
        // 正确的做法是：跨线程才 dispatch，同线程必须立即同步执行！

        if (std::this_thread::get_id() != m_impl->uiThreadId) {
            m_impl->w->dispatch([this, id, additionalData]() { PostSharedMemoryToWeb(id, additionalData); });
            return true;
        }

#ifdef _WIN32
        auto it = m_impl->sharedMemories.find(id);
        if (it == m_impl->sharedMemories.end() || !it->second.sharedBuffer) return false;

        auto controller = (ICoreWebView2Controller*)GetNativeController();
        if (!controller) return false;

        Microsoft::WRL::ComPtr<ICoreWebView2> wv2;
        if (FAILED(controller->get_CoreWebView2(&wv2))) return false;

        Microsoft::WRL::ComPtr<ICoreWebView2_17> wv2_17;
        if (FAILED(wv2.As(&wv2_17))) return false;

        int size_needed = MultiByteToWideChar(CP_UTF8, 0, additionalData.c_str(), (int)additionalData.size(), NULL, 0);
        std::wstring wadd(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, additionalData.c_str(), (int)additionalData.size(), &wadd[0], size_needed);

        HRESULT hr = wv2_17->PostSharedBufferToScript(
            it->second.sharedBuffer.Get(),
            COREWEBVIEW2_SHARED_BUFFER_ACCESS_READ_WRITE, 
            wadd.c_str()
        );
        
        if (FAILED(hr)) {
            // 如果你开了调试器，甚至可以在这里打印具体的错误码
            return false;
        }
        return true;
#endif
        return false;
    }

    void* WebviewWrapper::GetSharedMemory(int id) const {
#ifdef _WIN32
        auto it = m_impl->sharedMemories.find(id);
        if (it != m_impl->sharedMemories.end()) {
            return it->second.mappedSharedMemory;
        }
#endif
        return nullptr;
    }

    size_t WebviewWrapper::GetSharedMemorySize(int id) const {
#ifdef _WIN32
        auto it = m_impl->sharedMemories.find(id);
        if (it != m_impl->sharedMemories.end()) {
            return it->second.sharedMemorySize;
        }
#endif
        return 0;
    }

    bool WebviewWrapper::DestroySharedMemory(int id) {
        if (!m_impl->isInitialized) return false;

#ifdef _WIN32
        auto it = m_impl->sharedMemories.find(id);
        if (it != m_impl->sharedMemories.end()) {
            if (it->second.mappedSharedMemory) {
                UnmapViewOfFile(it->second.mappedSharedMemory);
            }
            if (it->second.sharedBuffer) {
                it->second.sharedBuffer->Close();
            }
            m_impl->sharedMemories.erase(it);
            return true;
        }
#endif
        return false;
    }

}
}