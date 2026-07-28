#include "MemoManager.hpp"
#include <Config.hpp>
#include <Log.hpp>
#include <filesystem>
#include <windows.h>

namespace Shin::Data {

    MemoManager& MemoManager::GetInstance() {
        static MemoManager instance;
        return instance;
    }

    bool MemoManager::Initialize(Shin::Core::Config& config) {
        std::string dbName = config.GetValue("Calendar.db_path", "memos.db");
        
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring strExePath(exePath);
        size_t pos = strExePath.find_last_of(L"\\/");
        std::wstring exeDir = strExePath.substr(0, pos);
        std::wstring dbPath = exeDir + L"\\" + std::wstring(dbName.begin(), dbName.end());

        if (!std::filesystem::exists(dbPath)) {
            Shin::LOGW("Calendar") << "Database file not found: " << dbName << ". Calendar module offline.";
            m_initialized = false;
            return false;
        }

        try {
            m_db = std::make_unique<SQLite::Database>(std::string(dbPath.begin(), dbPath.end()), SQLite::OPEN_READWRITE);
            m_initialized = true;
            Shin::LOGI("Calendar") << "Calendar database loaded: " << dbName;
            return true;
        } catch (const std::exception& e) {
            Shin::LOGE("Calendar") << "Failed to open database: " << e.what();
            m_initialized = false;
            return false;
        }
    }

    nlohmann::json MemoManager::ExecuteQuery(const std::string& sql) {
        nlohmann::json result = nlohmann::json::array();
        if (!m_initialized) return result;

        try {
            SQLite::Statement query(*m_db, sql);
            while (query.executeStep()) {
                nlohmann::json row;
                for (int i = 0; i < query.getColumnCount(); ++i) {
                    row[query.getColumnName(i)] = query.getColumn(i).getText();
                }
                result.push_back(row);
            }
        } catch (const std::exception& e) {
            Shin::LOGE("Calendar") << "Query Error: " << e.what();
        }
        return result;
    }

    int MemoManager::Execute(const std::string& sql) {
        if (!m_initialized) return -1;
        try {
            return m_db->exec(sql);
        } catch (const std::exception& e) {
            Shin::LOGE("Calendar") << "Execute Error: " << e.what();
            return -1;
        }
    }
}
