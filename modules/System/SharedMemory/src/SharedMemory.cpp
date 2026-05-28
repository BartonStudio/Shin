#include "SharedMemory.hpp"
#include <windows.h>
#include <stdexcept>

namespace Shin {
namespace System {

    SharedMemory::SharedMemory(const std::string& name, size_t size) 
        : m_name(name), m_size(size), m_hMapFile(nullptr), m_pBuffer(nullptr), m_hMutex(nullptr) {
        
        std::string mutexName = name + "_Mutex";
        m_hMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
        if (!m_hMutex) {
            throw std::runtime_error("Failed to create or open Named Mutex: " + mutexName);
        }

        m_hMapFile = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,                      
            static_cast<DWORD>(size), 
            name.c_str()
        );

        if (!m_hMapFile) {
            CloseHandle(m_hMutex);
            throw std::runtime_error("Failed to create or open Shared Memory: " + name);
        }

        m_pBuffer = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (!m_pBuffer) {
            CloseHandle(m_hMapFile);
            CloseHandle(m_hMutex);
            throw std::runtime_error("Failed to map Shared Memory view: " + name);
        }
    }

    SharedMemory::~SharedMemory() {
        if (m_pBuffer) {
            UnmapViewOfFile(m_pBuffer);
            m_pBuffer = nullptr;
        }
        if (m_hMapFile) {
            CloseHandle(m_hMapFile);
            m_hMapFile = nullptr;
        }
        if (m_hMutex) {
            CloseHandle(m_hMutex);
            m_hMutex = nullptr;
        }
    }

    size_t SharedMemory::GetSize() const { return m_size; }
    std::string SharedMemory::GetName() const { return m_name; }

    bool SharedMemory::Lock(int timeoutMs) {
        if (!m_hMutex) return false;
        DWORD timeout = (timeoutMs < 0) ? INFINITE : static_cast<DWORD>(timeoutMs);
        DWORD result = WaitForSingleObject(m_hMutex, timeout);
        return (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED);
    }

    void SharedMemory::Unlock() {
        if (m_hMutex) {
            ReleaseMutex(m_hMutex);
        }
    }

    void* SharedMemory::GetBuffer() {
        return m_pBuffer;
    }

    bool SharedMemory::Write(size_t offset, const void* data, size_t length, int timeoutMs) {
        if (offset + length > m_size) return false; 
        if (!data) return false;

        if (Lock(timeoutMs)) {
            char* dest = static_cast<char*>(m_pBuffer) + offset;
            memcpy(dest, data, length);
            Unlock();
            return true;
        }
        return false;
    }

    bool SharedMemory::Read(size_t offset, void* dest, size_t length, int timeoutMs) {
        if (offset + length > m_size) return false; 
        if (!dest) return false;

        if (Lock(timeoutMs)) {
            const char* src = static_cast<const char*>(m_pBuffer) + offset;
            memcpy(dest, src, length);
            Unlock();
            return true;
        }
        return false;
    }

}
}