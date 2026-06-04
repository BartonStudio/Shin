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
    Shin::UI::WebviewMessageHandler::RegisterAction("TriggerAuth", [](const nlohmann::json& req, nlohmann::json res, Shin::UI::WebviewMessageHandler::ResponseCallback sendResponse) {
        Shin::LOGI("BusinessLayer") << "Received TriggerAuth request from JS.";

        Shin::System::AuthOptions options;
        options.promptMessage = L"Please verify your identity for Shin Security Test.";
        options.allowBypassIfUnconfigured = true;
        
        options.parentWindowHandle = static_cast<HWND>(Shin::UI::WebviewWrapper::GetInstance().GetNativeWindow());

        auto authResult = Shin::System::LocalAuthenticator::VerifyUser(options);
        std::string resultStr = AuthResultToString(authResult);

        Shin::LOGI("BusinessLayer") << "Auth result: " << resultStr;

        res["result"] = resultStr;
        sendResponse(res);
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