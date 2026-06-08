#pragma once
#include <string>

namespace Shin {
namespace Core {

    /**
     * @brief Interface for shared memory management.
     * This breaks circular dependencies between Webview (UI) and Service layers.
     */
    class ISharedMemoryManager {
    public:
        virtual ~ISharedMemoryManager() = default;
        virtual void* CreateSharedMemory(int& outId, size_t size) = 0;
        virtual bool DestroySharedMemory(int id) = 0;
        virtual void* GetSharedMemory(int id) const = 0;
        virtual size_t GetSharedMemorySize(int id) const = 0;
        virtual void SendJson(const std::string& json) = 0;
        virtual bool PostSharedMemoryToWeb(int id, const std::string& additionalData) = 0;
    };

} // namespace Core
} // namespace Shin
