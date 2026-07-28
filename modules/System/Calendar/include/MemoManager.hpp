#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <SQLiteCpp/SQLiteCpp.h>
#include <nlohmann/json.hpp>
#include <Log.hpp>
#include <filesystem>
#include <windows.h>

#ifdef SHIN_SYSTEM_EXPORTS
    #define SHIN_SYSTEM_API __declspec(dllexport)
#else
    #define SHIN_SYSTEM_API __declspec(dllimport)
#endif

namespace Shin::Core { class Config; }

namespace Shin::Data {

    class SHIN_SYSTEM_API MemoManager {
    public:
        static MemoManager& GetInstance();
        
        bool Initialize(Shin::Core::Config& config);
        nlohmann::json ExecuteQuery(const std::string& sql);
        int Execute(const std::string& sql);

    private:
        MemoManager() = default;
        ~MemoManager() = default;
        
        std::unique_ptr<SQLite::Database> m_db;
        bool m_initialized = false;
    };
}
