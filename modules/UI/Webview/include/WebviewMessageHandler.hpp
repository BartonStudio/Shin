#pragma once
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

#ifdef _WIN32
    #ifdef SHIN_UIWEBVIEW_EXPORTS
        #define SHIN_UIWEBVIEW_API __declspec(dllexport)
    #else
        #define SHIN_UIWEBVIEW_API __declspec(dllimport)
    #endif
#else
    #define SHIN_UIWEBVIEW_API __attribute__((visibility("default")))
#endif

namespace Shin { 
    namespace UI { 
        class WebviewWrapper; 
        namespace WebviewMessageHandler { 
            SHIN_UIWEBVIEW_API void NotifySharedMemoryUpdate(int id); 
            using ResponseCallback = std::function<void(const nlohmann::json&)>;
            using ActionHandler = std::function<void(const nlohmann::json& req, nlohmann::json res, ResponseCallback sendResponse)>; 
            SHIN_UIWEBVIEW_API void RegisterAction(const std::string& actionName, ActionHandler handler, bool runInBackground = true); 
            using SharedMemoryUpdateObserver = std::function<void(int id)>; 
            SHIN_UIWEBVIEW_API void AddSharedMemoryUpdateObserver(SharedMemoryUpdateObserver observer); 
            SHIN_UIWEBVIEW_API void ClearSharedMemoryUpdateObservers(); 
        } 
    } 
}
