#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <TaskLoop.hpp>
#include <windows.h>
#include <functional>
#include <ISharedMemoryManager.hpp>
#include <atomic>

namespace Shin {
namespace Service {

    /**
     * @brief Manages camera stream connections, decoding, and shared memory mapping.
     */
    class VideoService {
    public:
        static VideoService& GetInstance();
        
        // 用于回调前端的回调接口
        using CreateShmCallback = std::function<void*(int&, size_t)>;
        using SendJsonCallback = std::function<void(const std::string&)>;
        using DestroyShmCallback = std::function<bool(int)>;
        using GetShmCallback = std::function<void*(int)>;

        void SetCallbacks(CreateShmCallback create, SendJsonCallback send, DestroyShmCallback destroy, GetShmCallback get) {
            m_createShm = create;
            m_sendJson = send;
            m_destroyShm = destroy;
            m_getShm = get;
        }

        void SetManager(Core::ISharedMemoryManager* manager);

        int ConnectStream(const std::string& ip, int port, 
                          const std::string& username, const std::string& password,
                          const std::string& streamPath = "", 
                          const std::string& transportProtocol = "TCP", 
                          const std::string& businessProtocol = "");

        void DisconnectStream(int id);

    private:
        VideoService();
        ~VideoService();
        VideoService(const VideoService&) = delete;
        VideoService& operator=(const VideoService&) = delete;

        // 回调成员
        CreateShmCallback m_createShm;
        SendJsonCallback m_sendJson;
        DestroyShmCallback m_destroyShm;
        GetShmCallback m_getShm;

        // 回调成员
        Core::ISharedMemoryManager* m_manager = nullptr;

        struct StreamContext {
            int sharedMemId = -1;
            std::atomic<bool> running;
            long userId = -1;
            long playHandle = -1;
            long playPort = -1;
            bool sharedMemoryInitialized = false;
        };

        std::mutex m_mutex;
        std::unordered_map<int, std::unique_ptr<StreamContext>> m_streams;
        
        static std::unordered_map<long, int> s_handleToId;
        static std::mutex s_handleMutex;
        
        // 新的 SDK 回调
        static void __stdcall RealDataCallBack(long lPlayHandle, unsigned long dwDataType, unsigned char *pBuffer, unsigned long dwBufSize, void* pUser);
        static void __stdcall DecCallBack(long nPort, char* pBuf, long nSize, void* pFrameInfo, long nReserved1, long nReserved2);
        
        long GetFirstAvailableChannel(long userId);
    };

} // namespace Service
} // namespace Shin
