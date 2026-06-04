#include <ShinCore.hpp>
#include <WebviewWrapper.hpp>

/**
 * Shin Application - Main Entry Point
 */

int main() {
    // 1. Unified Engine Initialization
    // This will automatically initialize Log, Config, and all linked modules (like Webview)
    if (!Shin::Init()) {
        return -1;
    }

    // 2. Application Logic: Navigate to your entry page
    auto& webview = Shin::UI::WebviewWrapper::GetInstance();

    // 3. Start the main event loop
    webview.RunBlocking();

    // 3. Cleanup
    Shin::Shutdown();

    return 0;
}
