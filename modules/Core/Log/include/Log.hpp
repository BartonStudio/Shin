#pragma once

#include <sstream>
#include <utility>

// For static library, we don't need dllexport/dllimport.
#define SHIN_API

namespace Shin {
    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warning,
        Error
    };

    class SHIN_API Log {
    public:
        // Initialize the logging system
        static void Init();

        // Print a raw message string
        static void Print(LogLevel level, const char* tag, const char* msg);
    };

    // A temporary stream object that collects the log message via operator<<
    // and flushes it to the backend when destroyed.
    class LogStream {
    public:
        LogStream(LogLevel level, const char* tag) : m_level(level), m_tag(tag) {}
        
        // Prevent copying
        LogStream(const LogStream&) = delete;
        LogStream& operator=(const LogStream&) = delete;

        // Allow moving (required for returning by value in older C++ standards, 
        // though C++17 copy elision usually avoids this).
        LogStream(LogStream&& other) noexcept 
            : m_level(other.m_level), m_tag(other.m_tag), m_buffer(std::move(other.m_buffer)) {
            // Nullify the tag on the moved-from object so it doesn't print twice
            other.m_tag = nullptr;
        }

        ~LogStream() {
            if (m_tag) {
                Log::Print(m_level, m_tag, m_buffer.str().c_str());
            }
        }

        template <typename T>
        LogStream& operator<<(const T& value) {
            m_buffer << value;
            return *this;
        }

    private:
        LogLevel m_level;
        const char* m_tag;
        std::ostringstream m_buffer;
    };

    // Inline helpers to start a stream expression.
    inline LogStream LOGT(const char* tag) { return LogStream(LogLevel::Trace, tag); }
    inline LogStream LOGD(const char* tag) { return LogStream(LogLevel::Debug, tag); }
    inline LogStream LOGI(const char* tag) { return LogStream(LogLevel::Info, tag); }
    inline LogStream LOGW(const char* tag) { return LogStream(LogLevel::Warning, tag); }
    inline LogStream LOGE(const char* tag) { return LogStream(LogLevel::Error, tag); }
}