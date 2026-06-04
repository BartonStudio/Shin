#include <toml.hpp>
#include <iostream>
#include <Log.hpp>

int main() {
    Shin::Log::Init();
    Shin::LOGI("TomlTest") << "Starting toml11 integration test...";

    try {
        // Create a dummy TOML string with comments
        const auto data = toml::parse(R"(
            # This is a comment
            [server]
            ip = "127.0.0.1" # The server IP
            port = 8080
        )");

        // Read values
        std::string ip = toml::find<std::string>(data, "server", "ip");
        int port = toml::find<int>(data, "server", "port");

        Shin::LOGI("TomlTest") << "Parsed Config - IP: " << ip << ", Port: " << port;

        // Verify comments (preserving comments)
        // Note: toml11 v4 preserves comments by default in toml::value
        const auto data_with_comments = toml::parse(R"(
            # Main Server Config
            [server]
            port = 8080 # default port
        )");
        
        Shin::LOGI("TomlTest") << "Serialized with comments:\n" << toml::format(data_with_comments);

    } catch (const std::exception& e) {
        Shin::LOGE("TomlTest") << "Error: " << e.what();
        return 1;
    }

    Shin::LOGI("TomlTest") << "toml11 integration test passed!";
    return 0;
}
