#include "WebviewMessageHandler.hpp"
#include "WebviewWrapper.hpp"
#include <Log.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>

namespace Shin {
namespace UI {
namespace WebviewMessageHandler {

    std::string ProcessMessage(const std::string& jsonRequest);

    namespace {
        using ActionHandler = std::function<void(const nlohmann::json&, nlohmann::json&, WebviewWrapper&)>;
        
        // 专门存放 SharedMemoryUpdate 的观察者列表
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

        void HandleCreateSharedMemory(const nlohmann::json& req, nlohmann::json& res, WebviewWrapper& webview) {
            Shin::LOGI("WebviewMessageHandler") << "Handling CreateSharedMemory...";
            
            if (!req.contains("size") || !req["size"].is_number_integer()) {
                res["action"] = "ErrorReport";
                res["msg"] = "CreateSharedMemory 请求参数错误，必须包含整型的 'size'";
                webview.SendJson(res.dump());
                return;
            }

            size_t size = req["size"].get<size_t>();

            int id = -1;
            void* ptr = webview.CreateSharedMemory(id, size);
            if (!ptr || id == -1) {
                res["action"] = "ErrorReport";
                res["msg"] = "创建共享内存失败 (size: " + std::to_string(size) + ")";
                webview.SendJson(res.dump());
                return;
            }

            res["id"] = id;
            res["size"] = size;

            bool postOk = webview.PostSharedMemoryToWeb(id, res.dump());
            if (!postOk) {
                res.erase("id");
                res.erase("size");
                res["action"] = "ErrorReport";
                res["msg"] = "发送共享内存句柄给前端失败 (PostSharedMemoryToWeb failed)";
                webview.SendJson(res.dump());
            }
        }

        void HandleDestroySharedMemory(const nlohmann::json& req, nlohmann::json& res, WebviewWrapper& webview) {
            Shin::LOGI("WebviewMessageHandler") << "Handling DestroySharedMemory...";

            if (!req.contains("id") || !req["id"].is_number_integer()) {
                res["action"] = "ErrorReport";
                res["msg"] = "DestroySharedMemory 请求参数错误，必须包含整型的 'id'";
                webview.SendJson(res.dump());
                return;
            }

            int id = req["id"].get<int>();
            
            if (webview.DestroySharedMemory(id)) {
                res["id"] = id;
                webview.SendJson(res.dump());
            } else {
                res["action"] = "ErrorReport";
                res["msg"] = "销毁共享内存失败，指定的 id 不存在或已被销毁: " + std::to_string(id);
                webview.SendJson(res.dump());
            }
        }

        void HandleGetSharedMemory(const nlohmann::json& req, nlohmann::json& res, WebviewWrapper& webview) {
            Shin::LOGI("WebviewMessageHandler") << "Handling GetSharedMemory...";

            if (!req.contains("id") || !req["id"].is_number_integer()) {
                res["action"] = "ErrorReport";
                res["msg"] = "GetSharedMemory 请求参数错误，必须包含整型的 'id'";
                webview.SendJson(res.dump());
                return;
            }

            int id = req["id"].get<int>();
            size_t size = webview.GetSharedMemorySize(id);

            if (size == 0) {
                res["action"] = "ErrorReport";
                res["msg"] = "获取共享内存失败，指定的 id 不存在: " + std::to_string(id);
                webview.SendJson(res.dump());
                return;
            }

            res["id"] = id;
            res["size"] = size;

            bool postOk = webview.PostSharedMemoryToWeb(id, res.dump());
            if (!postOk) {
                res.erase("id");
                res.erase("size");
                res["action"] = "ErrorReport";
                res["msg"] = "发送共享内存句柄给前端失败 (PostSharedMemoryToWeb failed)";
                webview.SendJson(res.dump());
            }
        }

        void HandleSharedMemoryUpdate(const nlohmann::json& req, nlohmann::json& res, WebviewWrapper& webview) {
            if (!req.contains("id") || !req["id"].is_number_integer()) return;
            int id = req["id"].get<int>();
            
            void* ptr = webview.GetSharedMemory(id);
            if (ptr) {
                // 通知所有注册的业务模块
                if (!s_updateObservers.empty()) {
                    for (const auto& observer : s_updateObservers) {
                        observer(id);
                    }
                } else {
                    // 如果没有外部监听者，默认打个日志
                    size_t size = webview.GetSharedMemorySize(id);
                    std::string content(static_cast<const char*>(ptr), strnlen(static_cast<const char*>(ptr), size));
                    Shin::LOGI("WebviewMessageHandler") << "[JS -> C++] 收到前端主动推送，ID " << id << " 内容已更新: " << content;
                }
            }
        }

        static std::unordered_map<std::string, ActionHandler> s_actionHandlers = {
            { "CreateSharedMemory", HandleCreateSharedMemory },
            { "DestroySharedMemory", HandleDestroySharedMemory },
            { "GetSharedMemory", HandleGetSharedMemory },
            { "SharedMemoryUpdate", HandleSharedMemoryUpdate }
        };
    }

    void RegisterAction(const std::string& actionName, ActionHandler handler) {
        s_actionHandlers[actionName] = handler;
        Shin::LOGI("WebviewMessageHandler") << "Registered new custom action handler: " << actionName;
    }

    std::string ProcessMessage(const std::string& jsonRequest) {
        Shin::LOGI("WebviewMessageHandler") << "Received request from JS: " << jsonRequest;
        
        auto& webview = WebviewWrapper::GetInstance();
        nlohmann::json parsedJson;

        if (!TryParseRequest(jsonRequest, parsedJson, webview)) {
            return "{}";
        }

        // 核心修复：剥离 WebView 引擎强制附加的外层数组包装
        // JS 传过来的参数会被包装成 ["arg1", "arg2"], 这里我们提取真正的业务对象 arg1
        if (parsedJson.is_array() && !parsedJson.empty()) {
            parsedJson = parsedJson[0];
        }

        // 应对极端情况：剥开数组后，如果里面包的不是对象，而是个字符串（比如 JS 传参数时手贱用了 JSON.stringify()）
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
        
        nlohmann::json resBase = nlohmann::json::object();
        resBase["action"] = action;
        if (parsedJson.contains("msgIndex")) {
            resBase["msgIndex"] = parsedJson["msgIndex"];
        }

        auto it = s_actionHandlers.find(action);
        if (it != s_actionHandlers.end()) {
            it->second(parsedJson, resBase, webview);
            // 如果业务层没有清空 resBase（说明它想要网关代为回复）
            // 注意：目前我们在底层业务如 CreateSharedMemory 也是自己调用 webview 方法回复的
            // 所以这里什么都不用做
        } else {
            resBase["action"] = "ErrorReport";
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
