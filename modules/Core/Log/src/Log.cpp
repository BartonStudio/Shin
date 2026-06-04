#include "Log.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/base_sink.h>
#include <nlohmann/json.hpp>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <mutex>
#include <vector>
#include <chrono>

namespace Shin {
    static std::shared_ptr<spdlog::logger> s_logger;
    static std::shared_ptr<spdlog::sinks::dist_sink_mt> s_distSink;

    // Helper to convert spdlog level to string and Shin enum
    static std::pair<LogLevel, std::string> ConvertLevel(spdlog::level::level_enum spdLevel) {
        switch (spdLevel) {
            case spdlog::level::trace:    return {LogLevel::Trace, "TRACE"};
            case spdlog::level::debug:    return {LogLevel::Debug, "DEBUG"};
            case spdlog::level::info:     return {LogLevel::Info, "INFO"};
            case spdlog::level::warn:     return {LogLevel::Warning, "WARN"};
            case spdlog::level::err:      return {LogLevel::Error, "ERROR"};
            case spdlog::level::critical: return {LogLevel::Error, "FATAL"};
            default:                      return {LogLevel::Info, "INFO"};
        }
    }

    // A custom sink for structured callbacks
    template<typename Mutex>
    class structured_sink : public spdlog::sinks::base_sink<Mutex> {
    public:
        explicit structured_sink(Log::JsonLogCallback callback) : m_callback(std::move(callback)) {}
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            auto [level, levelStr] = ConvertLevel(msg.level);
            
            // Extract original tag and message if possible. 
            // Since we prepended [TAG] in Log::Print, we try to parse it back for structured data.
            std::string fullMsg(msg.payload.data(), msg.payload.size());
            std::string tag = "Log";
            std::string content = fullMsg;

            if (fullMsg.size() > 2 && fullMsg[0] == '[') {
                size_t pos = fullMsg.find("] ");
                if (pos != std::string::npos) {
                    tag = fullMsg.substr(1, pos - 1);
                    content = fullMsg.substr(pos + 2);
                }
            }

            Log::LogEntry entry;
            entry.level = level;
            entry.levelStr = levelStr;
            entry.tag = tag;
            entry.message = content;
            entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                msg.time.time_since_epoch()).count();

            // Create JSON
            nlohmann::json j;
            j["type"] = "log";
            j["level"] = entry.levelStr;
            j["tag"] = entry.tag;
            j["message"] = entry.message;
            j["timestamp"] = entry.timestamp;

            m_callback(j.dump(), entry);
        }

        void flush_() override {}
    private:
        Log::JsonLogCallback m_callback;
    };

    // A custom sink that calls a simple std::function
    template<typename Mutex>
    class callback_sink : public spdlog::sinks::base_sink<Mutex> {
    public:
        explicit callback_sink(Log::LogCallback callback) : m_callback(std::move(callback)) {}
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            auto [level, levelStr] = ConvertLevel(msg.level);
            std::string payload(msg.payload.data(), msg.payload.size());
            m_callback(level, "Log", payload.c_str());
        }
        void flush_() override {}
    private:
        Log::LogCallback m_callback;
    };

    void Log::Init() {
        static std::once_flag initFlag;
        std::call_once(initFlag, []() {
            // Create a distribution sink that can hold multiple sinks
            s_distSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
            
            // Add default console sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            s_distSink->add_sink(console_sink);
            
            // Create the logger with the distribution sink
            s_logger = std::make_shared<spdlog::logger>("ShinCore", s_distSink);
            
            // Set log pattern: [Time] [Level] Message
            // Note: %v is the message, which will include the [TAG] prepended in Log::Print
            s_logger->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l]%$ %v");
            
            // Default level
            s_logger->set_level(spdlog::level::trace);

            // Register as default
            spdlog::set_default_logger(s_logger);
        });
    }

    void Log::Print(LogLevel level, const char* tag, const char* msg) {
        Init();

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

    void Log::AddSink(std::shared_ptr<spdlog::sinks::sink> sink) {
        Init();
        s_distSink->add_sink(sink);
    }

    void Log::AddCallbackSink(LogCallback callback) {
        Init();
        auto sink = std::make_shared<callback_sink<std::mutex>>(std::move(callback));
        s_distSink->add_sink(sink);
    }

    void Log::AddJsonCallbackSink(JsonLogCallback callback) {
        Init();
        auto sink = std::make_shared<structured_sink<std::mutex>>(std::move(callback));
        s_distSink->add_sink(sink);
    }
}