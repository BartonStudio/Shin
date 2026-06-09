#include <ShinCore.hpp>
#include <WebviewWrapper.hpp>
#include <WebviewMessageHandler.hpp>
#include <VideoService.hpp>

/**
 * Shin Application - Main Entry Point
 */

int main() {
    // 1. Unified Engine Initialization
    if (!Shin::Init()) {
        return -1;
    }

    // 2. Setup Services and UI
    auto& webview = Shin::UI::WebviewWrapper::GetInstance();
    auto& videoService = Shin::Service::VideoService::GetInstance();

    // 编排依赖 (Dependency Injection)
    videoService.SetManager(&webview);

    // 注册业务 Action：处理来自前端的摄像头连接请求
    Shin::UI::WebviewMessageHandler::RegisterAction(
        "HikvisionStreamConnect",
        [](const nlohmann::json& req, nlohmann::json res, Shin::UI::WebviewMessageHandler::ResponseCallback sendResponse) {
            std::string ip = req.value("ip", "");
            int port = req.value("port", 8000);
            std::string username = req.value("username", "");
            std::string password = req.value("password", "");

            if (ip.empty() || username.empty()) {
                res["action"] = "ErrorReport";
                res["msg"] = "IP 或用户名不能为空";
                sendResponse(res);
                return;
            }

            int id = Shin::Service::VideoService::GetInstance().ConnectStream(ip, port, username, password, "", "UDP");
            if (id == -1) {
                res["action"] = "ErrorReport";
                res["msg"] = "连接海康摄像头失败";
                sendResponse(res);
            } else {
                res["id"] = id;
                res["msg"] = "连接指令已发送";
                sendResponse(res);
            }
        },
        true
    );

    // 注册业务 Action：断开摄像头连接
    Shin::UI::WebviewMessageHandler::RegisterAction(
        "HikvisionStreamDisconnect",
        [](const nlohmann::json& req, nlohmann::json res, Shin::UI::WebviewMessageHandler::ResponseCallback sendResponse) {
            int id = req.value("id", -1);
            if (id != -1) {
                Shin::Service::VideoService::GetInstance().DisconnectStream(id);
                res["msg"] = "已断开连接";
                sendResponse(res);
            } else {
                res["action"] = "ErrorReport";
                res["msg"] = "无效的流 ID";
                sendResponse(res);
            }
        },
        true
    );

    // 3. Start the main event loop
    webview.RunBlocking();

    // 3. Cleanup
    Shin::Shutdown();

    return 0;
}
