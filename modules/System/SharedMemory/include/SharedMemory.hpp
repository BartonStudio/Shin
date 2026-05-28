#pragma once
#include <string>
#include <cstdint>

#ifdef _WIN32
    #ifdef SHIN_SYSTEM_EXPORTS
        #define SHIN_SYSTEM_API __declspec(dllexport)
    #else
        #define SHIN_SYSTEM_API __declspec(dllimport)
    #endif
#else
    #define SHIN_SYSTEM_API __attribute__((visibility("default")))
#endif

// Disable C4251 for std::string crossing DLL boundary
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace Shin {
namespace System {

    class SHIN_SYSTEM_API SharedMemory {
    public:
        SharedMemory(const std::string& name, size_t size);
        ~SharedMemory();

        SharedMemory(const SharedMemory&) = delete;
        SharedMemory& operator=(const SharedMemory&) = delete;

        size_t GetSize() const;
        std::string GetName() const;
        
        bool Lock(int timeoutMs = -1);
        void Unlock();
        void* GetBuffer();

        bool Write(size_t offset, const void* data, size_t length, int timeoutMs = 100);
        bool Read(size_t offset, void* dest, size_t length, int timeoutMs = 100);

    private:
        std::string m_name;
        size_t m_size;
        
        void* m_hMapFile;    
        void* m_pBuffer;     
        void* m_hMutex;      
    };

}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif