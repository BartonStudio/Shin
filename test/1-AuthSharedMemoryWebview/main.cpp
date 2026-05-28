#include <WebviewWrapper.hpp>
#include <WebviewMessageHandler.hpp>
#include <LocalAuthenticator.hpp>
#include <Log.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
std::filesystem::path GetExecutableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}
#endif

std::string AuthResultToString(Shin::System::AuthResult result) {
    switch (result) {
        case Shin::System::AuthResult::Success:     return "Success";
        case Shin::System::AuthResult::Bypassed:    return "Bypassed";
        case Shin::System::AuthResult::Canceled:    return "Canceled";
        case Shin::System::AuthResult::Failed:      return "Failed";
        case Shin::System::AuthResult::Unavailable: return "Unavailable";
        case Shin::System::AuthResult::Error:       return "Error";
        default:                                    return "Unknown";
    }
}

int main() {
    Shin::LOGI("Main") << "=== Shin Test: 1-AuthSharedMemoryWebview ===";

    auto& webview = Shin::UI::WebviewWrapper::GetInstance();

    webview.SetTitle("Authentication & Webview Integration Test");
    webview.SetSize(600, 450, true);
    webview.SetDebug(true);

    // ==========================================
    // 动态路由注册机制：外部业务接入
    // 告别底层的 BindFunction！直接向系统注册 Action 处理回调。
    // ==========================================
    Shin::UI::WebviewMessageHandler::RegisterAction("TriggerAuth", [&webview](const nlohmann::json& req, nlohmann::json& res, Shin::UI::WebviewWrapper& wv) {
        Shin::LOGI("BusinessLayer") << "Received TriggerAuth request from JS.";

        std::string msgIndex = req.contains("msgIndex") ? req["msgIndex"].get<std::string>() : "";

        // 解法：利用 std::thread 开一个独立的后台线程，在这个线程里强制重新初始化 WinRT 的 MTA 套间
        std::thread([&webview, msgIndex]() {
            Shin::System::AuthOptions options;
            options.promptMessage = L"Please verify your identity for Shin Security Test.";
            options.allowBypassIfUnconfigured = true;
            // 注意：跨线程调用 GetNativeWindow 获取 HWND 是安全的
            options.parentWindowHandle = static_cast<HWND>(webview.GetNativeWindow());

            // 调用原生认证
            auto authResult = Shin::System::LocalAuthenticator::VerifyUser(options);
            std::string resultStr = AuthResultToString(authResult);

            Shin::LOGI("BusinessLayer") << "Auth result: " << resultStr;

            // 构造响应 JSON
            nlohmann::json threadRes = {
                {"action", "AuthResponse"},
                {"result", resultStr}
            };
            if (!msgIndex.empty()) {
                threadRes["msgIndex"] = msgIndex;
            }

            // 跨线程安全地推送回前端
            webview.SendJson(threadRes.dump());
        }).detach();

        // 由于上面开了子线程异步处理并自行发送了 AuthResponse
        // 当前这个回调直接让它走完即可。因为这是一个特殊的 RPC，
        // 为了防止底层自动发一条空结果回去覆盖前端状态，我们清空 action 不让底层路由回复它
        res.clear();
    });

    auto htmlPath = GetExecutableDir() / "AuthTest.html";
    std::string url = "file:///" + htmlPath.generic_string();
    Shin::LOGI("Main") << "Loading URL: " << url;
    webview.SetStartupURL(url);

    if (!webview.Initialize()) {
        Shin::LOGE("Main") << "Failed to initialize Webview!";
        return -1;
    }

    Shin::LOGI("Main") << "Starting UI message loop (blocking)...";
    webview.RunBlocking();
    Shin::LOGI("Main") << "UI loop exited successfully.";

    return 0;
}