#include "Log.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <mutex>

namespace Shin {
    static std::shared_ptr<spdlog::logger> s_logger;

    void Log::Init() {
        if (!s_logger) {
            // Create a default stdout color logger
            s_logger = spdlog::stdout_color_mt("ShinCore");
            
            // Set as default logger just in case
            spdlog::set_default_logger(s_logger);
            
            // Set log pattern: [Time] [Level] Message
            spdlog::set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l]%$ %v");
            
            // Default level
            s_logger->set_level(spdlog::level::trace);
        }
    }

    void Log::Print(LogLevel level, const char* tag, const char* msg) {
        // 使用 C++11 std::call_once 保证极其安全和高效的 Lazy Initialization
        static std::once_flag initFlag;
        std::call_once(initFlag, []() {
            Init();
        });

        // Format the final message with the TAG
        std::string formatted = std::string("[") + tag + "] " + msg;

        // Route to spdlog based on level
        switch (level) {
            case LogLevel::Trace:   s_logger->trace(formatted); break;
            case LogLevel::Debug:   s_logger->debug(formatted); break;
            case LogLevel::Info:    s_logger->info(formatted); break;
            case LogLevel::Warning: s_logger->warn(formatted); break;
            case LogLevel::Error:   s_logger->error(formatted); break;
        }
    }
}