#include "WebviewMessageHandler.hpp"
#include "WebviewWrapper.hpp"
#include <Log.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <windows.h>
#include <WebView2.h>
#include <wrl.h>

namespace Shin {
namespace UI {
namespace WebviewMessageHandler {

    std::string ProcessMessage(const std::string& jsonRequest);

    namespace {
        class SimpleThreadPool {
        public:
            SimpleThreadPool(size_t threads = 4) : stop(false) {
                for (size_t i = 0; i < threads; ++i) {
                    workers.emplace_back([this] {
                        for (;;) {
                            std::function<void()> task;
                            {
                                std::unique_lock<std::mutex> lock(this->queue_mutex);
                                this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                                if (this->stop && this->tasks.empty()) return;
                                task = std::move(this->tasks.front());
                                this->tasks.pop();
                            }
                            task();
                        }
                    });
                }
            }
            ~SimpleThreadPool() {
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    stop = true;
                }
                condition.notify_all();
                for (std::thread &worker : workers) {
                    worker.join();
                }
            }
            template<class F> void enqueue(F&& f) {
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    tasks.emplace(std::forward<F>(f));
                }
                condition.notify_one();
            }
        private:
            std::vector<std::thread> workers;
            std::queue<std::function<void()>> tasks;
            std::mutex queue_mutex;
            std::condition_variable condition;
            bool stop;
        };

        SimpleThreadPool s_threadPool(4);

        struct ActionDef {
            ActionHandler handler;
            bool runInBackground;
        };

        std::vector<SharedMemoryUpdateObserver> s_updateObservers;

        bool TryParseRequest(const std::string& raw, nlohmann::json& outJson, WebviewWrapper& webview) {
            try {
                outJson = nlohmann::json::parse(raw);
                return true;
            } catch (const nlohmann::json::parse_error& e) {
                Shin::LOGE("WebviewMessageHandler") << "Invalid JSON: " << e.what();
                nlohmann::json errResponse = {
                    {"action", "ErrorReport"},
                    {"msg", std::string("解析前端参数失败，不是合法的 JSON 格式: ") + e.what()}
                };
                webview.SendJson(errResponse.dump());
                return false;
            }
        }

        void HandleCreateSharedMemory(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            if (!req.contains("size") || !req["size"].is_number_integer()) {
                res["action"] = "ErrorReport";
                res["msg"] = "CreateSharedMemory 请求参数错误，必须包含整型的 'size'";
                sendResponse(res);
                return;
            }

            size_t size = req["size"].get<size_t>();
            int id = -1;
            void* ptr = WebviewWrapper::GetInstance().CreateSharedMemory(id, size);
            
            if (!ptr || id == -1) {
                res["action"] = "ErrorReport";
                res["msg"] = "创建共享内存失败 (size: " + std::to_string(size) + ")";
                sendResponse(res);
                return;
            }

            res["id"] = id;
            res["size"] = size;

            bool postOk = WebviewWrapper::GetInstance().PostSharedMemoryToWeb(id, res.dump());
            if (!postOk) {
                res.erase("id");
                res.erase("size");
                res["action"] = "ErrorReport";
                res["msg"] = "发送共享内存句柄给前端失败 (PostSharedMemoryToWeb failed)";
                sendResponse(res);
            }
        }

        void HandleDestroySharedMemory(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            if (!req.contains("id") || !req["id"].is_number_integer()) {
                res["action"] = "ErrorReport";
                res["msg"] = "DestroySharedMemory 请求参数错误，必须包含整型的 'id'";
                sendResponse(res);
                return;
            }

            int id = req["id"].get<int>();
            if (WebviewWrapper::GetInstance().DestroySharedMemory(id)) {
                res["id"] = id;
                sendResponse(res);
            } else {
                res["action"] = "ErrorReport";
                res["msg"] = "销毁共享内存失败，指定的 id 不存在或已被销毁: " + std::to_string(id);
                sendResponse(res);
            }
        }

        void HandleGetSharedMemory(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            if (!req.contains("id") || !req["id"].is_number_integer()) {
                res["action"] = "ErrorReport";
                res["msg"] = "GetSharedMemory 请求参数错误，必须包含整型的 'id'";
                sendResponse(res);
                return;
            }

            int id = req["id"].get<int>();
            size_t size = WebviewWrapper::GetInstance().GetSharedMemorySize(id);

            if (size == 0) {
                res["action"] = "ErrorReport";
                res["msg"] = "获取共享内存失败，指定的 id 不存在: " + std::to_string(id);
                sendResponse(res);
                return;
            }

            res["id"] = id;
            res["size"] = size;

            bool postOk = WebviewWrapper::GetInstance().PostSharedMemoryToWeb(id, res.dump());
            if (!postOk) {
                res.erase("id");
                res.erase("size");
                res["action"] = "ErrorReport";
                res["msg"] = "发送共享内存句柄给前端失败 (PostSharedMemoryToWeb failed)";
                sendResponse(res);
            }
        }

        void HandleSharedMemoryUpdate(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            if (!req.contains("id") || !req["id"].is_number_integer()) return;
            int id = req["id"].get<int>();
            
            void* ptr = WebviewWrapper::GetInstance().GetSharedMemory(id);
            if (ptr) {
                if (!s_updateObservers.empty()) {
                    for (const auto& observer : s_updateObservers) {
                        observer(id);
                    }
                } else {
                    size_t size = WebviewWrapper::GetInstance().GetSharedMemorySize(id);
                    std::string content(static_cast<const char*>(ptr), strnlen(static_cast<const char*>(ptr), size));
                    Shin::LOGI("WebviewMessageHandler") << "[JS -> C++] 收到前端主动推送，ID " << id << " 内容已更新: " << content;
                }
            }
        }

        void HandleWindowMinimize(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            HWND hwnd = (HWND)WebviewWrapper::GetInstance().GetNativeWindow();
            if (hwnd) {
                PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            }
        }

        void HandleWindowDrag(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            HWND hwnd = (HWND)WebviewWrapper::GetInstance().GetNativeWindow();
            if (hwnd) {
                ReleaseCapture();
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }

        void HandleWindowToggleMaximize(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            HWND hwnd = (HWND)WebviewWrapper::GetInstance().GetNativeWindow();
            if (hwnd) {
                if (IsZoomed(hwnd)) {
                    PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                } else {
                    PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                }
            }
        }

        void HandleWindowClose(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            HWND hwnd = (HWND)WebviewWrapper::GetInstance().GetNativeWindow();
            if (hwnd) {
                PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
            }
        }

        void HandleWindowOpenDevTools(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            auto controller = (ICoreWebView2Controller*)WebviewWrapper::GetInstance().GetNativeController();
            if (controller) {
                Microsoft::WRL::ComPtr<ICoreWebView2> wv2;
                if (SUCCEEDED(controller->get_CoreWebView2(&wv2))) {
                    wv2->OpenDevToolsWindow();
                }
            }
        }

        void HandleNavigate(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            std::string url = req.value("url", "");
            if (!url.empty()) {
                WebviewWrapper::GetInstance().Navigate(url);
            } else {
                res["action"] = "ErrorReport";
                res["msg"] = "Navigate URL is empty";
                sendResponse(res);
            }
        }

        void HandleWindowSetSize(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse) {
            if (!req.contains("width") || !req["width"].is_number_integer() ||
                !req.contains("height") || !req["height"].is_number_integer()) {
                return;
            }

            int width = req["width"].get<int>();
            int height = req["height"].get<int>();
            bool fixed = req.value("fixed", false);

            WebviewWrapper::GetInstance().SetSize(width, height, fixed);
        }

        static std::unordered_map<std::string, ActionDef> s_actionHandlers = {
            { "CreateSharedMemory", { HandleCreateSharedMemory, false } },
            { "DestroySharedMemory", { HandleDestroySharedMemory, false } },
            { "GetSharedMemory", { HandleGetSharedMemory, false } },
            { "SharedMemoryUpdate", { HandleSharedMemoryUpdate, false } },
            { "WindowMinimize", { HandleWindowMinimize, false } },
            { "WindowDrag", { HandleWindowDrag, false } },
            { "WindowToggleMaximize", { HandleWindowToggleMaximize, false } },
            { "WindowClose", { HandleWindowClose, false } },
            { "WindowOpenDevTools", { HandleWindowOpenDevTools, false } },
            { "Navigate", { HandleNavigate, false } },
            { "WindowSetSize", { HandleWindowSetSize, false } }
        };
    }

    void RegisterAction(const std::string& actionName, ActionHandler handler, bool runInBackground) {
        s_actionHandlers[actionName] = { handler, runInBackground };
        Shin::LOGI("WebviewMessageHandler") << "Registered new custom action handler: " << actionName << " (background=" << runInBackground << ")";
    }

    std::string ProcessMessage(const std::string& jsonRequest) {
        Shin::LOGI("WebviewMessageHandler") << "Received request from JS: " << jsonRequest;
        
        auto& webview = WebviewWrapper::GetInstance();
        nlohmann::json parsedJson;

        if (!TryParseRequest(jsonRequest, parsedJson, webview)) {
            return "{}";
        }

        if (parsedJson.is_array() && !parsedJson.empty()) {
            parsedJson = parsedJson[0];
        }

        if (parsedJson.is_string()) {
            try {
                parsedJson = nlohmann::json::parse(parsedJson.get<std::string>());
            } catch (...) {}
        }
        
        if (!parsedJson.contains("action") || !parsedJson["action"].is_string()) {
            nlohmann::json errResponse = {
                {"action", "ErrorReport"},
                {"msg", "请求中缺少 'action' 字段或其不是合法的字符串类型"}
            };
            if (parsedJson.is_object() && parsedJson.contains("msgIndex")) {
                errResponse["msgIndex"] = parsedJson["msgIndex"];
            }
            webview.SendJson(errResponse.dump());
            return "{}";
        }

        std::string action = parsedJson["action"].get<std::string>();
        std::string msgIndex = parsedJson.value("msgIndex", "");

        auto it = s_actionHandlers.find(action);
        if (it != s_actionHandlers.end()) {
            ResponseCallback sendResponse = [action, msgIndex](const nlohmann::json& responseData) {
                nlohmann::json resBase = responseData;
                // 如果业务侧自己指定了 action (如 AuthResponse)，就不覆盖；否则默认使用原请求的 action
                if (!resBase.contains("action")) {
                    resBase["action"] = action;
                }
                if (!msgIndex.empty() && !resBase.contains("msgIndex")) {
                    resBase["msgIndex"] = msgIndex;
                }
                // 安全获取实例并发送
                WebviewWrapper::GetInstance().SendJson(resBase.dump());
            };

            nlohmann::json initialRes;
            initialRes["action"] = action;
            if (!msgIndex.empty()) initialRes["msgIndex"] = msgIndex;

            if (it->second.runInBackground) {
                s_threadPool.enqueue([handler = it->second.handler, req = parsedJson, res = initialRes, sendResponse]() {
                    handler(req, res, sendResponse);
                });
            } else {
                it->second.handler(parsedJson, initialRes, sendResponse);
            }
        } else {
            nlohmann::json resBase;
            resBase["action"] = "ErrorReport";
            if (!msgIndex.empty()) resBase["msgIndex"] = msgIndex;
            resBase["msg"] = "未知的业务类型 (Unknown Action): " + action;
            webview.SendJson(resBase.dump());
        }
        
        return "{}";
    }

    void NotifySharedMemoryUpdate(int id) {
        auto& webview = WebviewWrapper::GetInstance();
        size_t size = webview.GetSharedMemorySize(id);
        if (size == 0) {
            Shin::LOGE("WebviewMessageHandler") << "NotifySharedMemoryUpdate failed: ID " << id << " not found.";
            return;
        }

        nlohmann::json res = {
            {"action", "SharedMemoryUpdate"},
            {"id", id},
            {"size", size}
        };

        bool postOk = webview.PostSharedMemoryToWeb(id, res.dump());
        if (!postOk) {
            Shin::LOGE("WebviewMessageHandler") << "NotifySharedMemoryUpdate failed: PostSharedMemoryToWeb failed.";
        } else {
            Shin::LOGI("WebviewMessageHandler") << "Pushed SharedMemoryUpdate to JS for ID " << id << ", new size: " << size;
        }
    }

    void AddSharedMemoryUpdateObserver(SharedMemoryUpdateObserver observer) {
        s_updateObservers.push_back(observer);
    }

    void ClearSharedMemoryUpdateObservers() {
        s_updateObservers.clear();
    }

}
}
}
