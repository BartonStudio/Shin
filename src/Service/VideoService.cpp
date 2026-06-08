#include "VideoService.hpp"
#include <Log.hpp>
#include <TaskLoop.hpp>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4828)
#endif
#include <HCNetSDK.h>
#include <plaympeg4.h>
#ifdef _WIN32
#pragma warning(pop)
#endif

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

namespace Shin {
namespace Service {

    std::unordered_map<long, int> VideoService::s_handleToId;
    std::mutex VideoService::s_handleMutex;

    VideoService& VideoService::GetInstance() {
        static VideoService instance;
        return instance;
    }

    VideoService::VideoService() {
        NET_DVR_Init();
        // 设置连接超时时间与重连功能
        NET_DVR_SetConnectTime(2000, 1);
        NET_DVR_SetReconnect(10000, true);
    }

    VideoService::~VideoService() {
        NET_DVR_Cleanup();
    }

    void VideoService::SetManager(Core::ISharedMemoryManager* manager) {
        m_manager = manager;
    }

    long VideoService::GetFirstAvailableChannel(long userId) {
        NET_DVR_IPPARACFG_V40 struIPParaCfg = {0};
        DWORD dwReturn = 0;
        if (NET_DVR_GetDVRConfig(userId, NET_DVR_GET_IPPARACFG_V40, 0, &struIPParaCfg, sizeof(NET_DVR_IPPARACFG_V40), &dwReturn)) {
            // 尝试找第一个启用的数字通道
            for (int i = 0; i < (int)struIPParaCfg.dwDChanNum; i++) {
                if (struIPParaCfg.byAnalogChanEnable[i]) {
                    return struIPParaCfg.dwStartDChan + i;
                }
            }
            // 尝试找第一个启用的模拟通道
            for (int i = 0; i < (int)struIPParaCfg.dwAChanNum; i++) {
                if (struIPParaCfg.byAnalogChanEnable[i]) {
                    return i + 1;
                }
            }
        }
        return 1; // 默认返回1
    }

    void __stdcall VideoService::RealDataCallBack(long lPlayHandle, unsigned long dwDataType, unsigned char *pBuffer, unsigned long dwBufSize, void* pUser) {
        int streamId = -1;
        {
            std::lock_guard<std::mutex> lock(s_handleMutex);
            if (s_handleToId.find(lPlayHandle) != s_handleToId.end()) {
                streamId = s_handleToId[lPlayHandle];
            }
        }
        if (streamId == -1) return;

        auto& service = VideoService::GetInstance();
        std::lock_guard<std::mutex> lock(service.m_mutex);
        auto it = service.m_streams.find(streamId);
        if (it == service.m_streams.end()) return;
        auto& context = it->second;

        if (dwDataType == NET_DVR_SYSHEAD) {
            if (context->playPort < 0) {
                if (PlayM4_GetPort(&context->playPort)) {
                    PlayM4_SetStreamOpenMode(context->playPort, STREAME_REALTIME);
                    PlayM4_OpenStream(context->playPort, pBuffer, dwBufSize, 2 * 1024 * 1024);
                    PlayM4_SetDisplayBuf(context->playPort, 1);
                    
                    PlayM4_SetDecCallBack(context->playPort, [](long nPort, char* pBuf, long nSize, FRAME_INFO* pFrameInfo, long nReserved1, long nReserved2) {
                        if (pFrameInfo->nType != T_YV12) return;

                        auto& svc = VideoService::GetInstance();
                        int sid = -1;
                        {
                            std::lock_guard<std::mutex> lock(svc.m_mutex);
                            for (auto& pair : svc.m_streams) {
                                if (pair.second->playPort == nPort) {
                                    // Check if we're already processing a frame for this stream
                                    if (pair.second->isProcessingFrame.exchange(true)) {
                                        return; // Drop the frame
                                    }
                                    sid = pair.first;
                                    break;
                                }
                            }
                        }
                        if (sid == -1) return;

                        cv::Mat yuv(pFrameInfo->nHeight * 3 / 2, pFrameInfo->nWidth, CV_8UC1, pBuf);
                        cv::Mat yuvCopy = yuv.clone();
                        int w = pFrameInfo->nWidth;
                        int h = pFrameInfo->nHeight;

                        Core::TaskLoop::GetInstance().PostTask([sid, yuvCopy, w, h]() {
                            auto& service = VideoService::GetInstance();
                            
                            // Use a unique_ptr-like pattern to ensure isProcessingFrame is reset
                            auto guard = std::shared_ptr<void>(nullptr, [&service, sid](void*) {
                                std::lock_guard<std::mutex> lock(service.m_mutex);
                                auto it = service.m_streams.find(sid);
                                if (it != service.m_streams.end()) {
                                    it->second->isProcessingFrame = false;
                                }
                            });

                            // 1. Initial check with minimal lock scope
                            bool needsInit = false;
                            size_t totalSize = 0;
                            {
                                std::lock_guard<std::mutex> lock(service.m_mutex);
                                auto it = service.m_streams.find(sid);
                                if (it == service.m_streams.end() || !it->second->running || !service.m_manager) return;
                                
                                if (!it->second->sharedMemoryInitialized) {
                                    needsInit = true;
                                    // Pre-calculate size while we have the context
                                    totalSize = (size_t)w * h * 3; // RGB elemSize is 3
                                }
                            }

                            // 2. Perform expensive color conversion outside the lock
                            cv::Mat rgb;
                            cv::cvtColor(yuvCopy, rgb, cv::COLOR_YUV2RGB_YV12);
                            totalSize = rgb.total() * rgb.elemSize();

                            auto& manager = *service.m_manager;

                            // 3. Handle initialization if needed, WITHOUT holding the lock
                            // because CreateSharedMemory might block waiting for the UI thread.
                            if (needsInit) {
                                int shmId = -1;
                                void* ptr = manager.CreateSharedMemory(shmId, totalSize);
                                if (ptr) {
                                    std::lock_guard<std::mutex> lock(service.m_mutex);
                                    auto it = service.m_streams.find(sid);
                                    if (it != service.m_streams.end() && it->second->running) {
                                        it->second->sharedMemId = shmId;
                                        it->second->sharedMemoryInitialized = true;
                                        
                                        nlohmann::json res;
                                        res["action"] = "HikvisionStreamConnect";
                                        res["id"] = shmId;
                                        res["size"] = totalSize;
                                        res["width"] = w;
                                        res["height"] = h;
                                        manager.PostSharedMemoryToWeb(shmId, res.dump());
                                    } else {
                                        // Stream was closed while we were creating memory
                                        manager.DestroySharedMemory(shmId);
                                    }
                                }
                            }

                            // 4. Update the memory content
                            std::lock_guard<std::mutex> lock(service.m_mutex);
                            auto it = service.m_streams.find(sid);
                            if (it != service.m_streams.end() && it->second->running && it->second->sharedMemoryInitialized) {
                                void* sharedMem = manager.GetSharedMemory(it->second->sharedMemId);
                                if (sharedMem) {
                                    memcpy(sharedMem, rgb.data, totalSize);
                                    nlohmann::json update;
                                    update["action"] = "SharedMemoryUpdate";
                                    update["id"] = it->second->sharedMemId;
                                    update["width"] = w;
                                    update["height"] = h;
                                    manager.PostSharedMemoryToWeb(it->second->sharedMemId, update.dump());
                                }
                            }
                        });
                    });

                    PlayM4_Play(context->playPort, NULL);
                }
            }
        } else if (dwDataType == NET_DVR_STREAMDATA) {
            if (context->playPort >= 0) {
                PlayM4_InputData(context->playPort, pBuffer, dwBufSize);
            }
        }
    }

    int VideoService::ConnectStream(const std::string& ip, int port, 
                                    const std::string& username, const std::string& password,
                                    const std::string& streamPath, 
                                    const std::string& transportProtocol, 
                                    const std::string& businessProtocol) {
        
        LOGI("VideoService") << "Connecting to Hikvision camera: " << ip << ":" << port;

        NET_DVR_USER_LOGIN_INFO loginInfo = {0};
        loginInfo.bUseAsynLogin = 0;
        strncpy_s(loginInfo.sDeviceAddress, ip.c_str(), NET_DVR_DEV_ADDRESS_MAX_LEN - 1);
        strncpy_s(loginInfo.sUserName, username.c_str(), NAME_LEN - 1);
        strncpy_s(loginInfo.sPassword, password.c_str(), PASSWD_LEN - 1);
        loginInfo.wPort = (unsigned short)port;

        NET_DVR_DEVICEINFO_V40 deviceInfo = {0};
        long userId = NET_DVR_Login_V40(&loginInfo, &deviceInfo);

        if (userId < 0) {
            LOGE("VideoService") << "Login failed. Error: " << NET_DVR_GetLastError();
            return -1;
        }

        long lChannel = GetFirstAvailableChannel(userId);
        LOGI("VideoService") << "Auto-detected channel: " << lChannel;

        NET_DVR_PREVIEWINFO previewInfo = {0};
        previewInfo.lChannel = lChannel;
        previewInfo.dwStreamType = 0; 
        previewInfo.dwLinkMode = (transportProtocol == "UDP") ? 1 : 0; 
        previewInfo.bBlocked = 1;
        previewInfo.dwDisplayBufNum = 1;

        long playHandle = NET_DVR_RealPlay_V40(userId, &previewInfo, &VideoService::RealDataCallBack, NULL);
        if (playHandle < 0) {
            LOGE("VideoService") << "RealPlay failed. Error: " << NET_DVR_GetLastError();
            NET_DVR_Logout(userId);
            return -1;
        }

        auto context = std::make_unique<StreamContext>();
        context->userId = userId;
        context->playHandle = playHandle;
        context->running = true;
        context->playPort = -1;

        std::lock_guard<std::mutex> lock(m_mutex);
        int streamId = (int)playHandle;
        
        {
            std::lock_guard<std::mutex> lockHandle(s_handleMutex);
            s_handleToId[playHandle] = streamId;
        }

        m_streams[streamId] = std::move(context);

        return streamId;
    }

    void VideoService::DisconnectStream(int id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_streams.find(id);
        if (it != m_streams.end()) {
            it->second->running = false;
            
            if (it->second->playPort >= 0) {
                PlayM4_Stop(it->second->playPort);
                PlayM4_CloseStream(it->second->playPort);
                PlayM4_FreePort(it->second->playPort);
            }

            NET_DVR_StopRealPlay(it->second->playHandle);
            NET_DVR_Logout(it->second->userId);
            
            if (m_manager) {
                m_manager->DestroySharedMemory(it->second->sharedMemId);
            }
            
            {
                std::lock_guard<std::mutex> lockHandle(s_handleMutex);
                s_handleToId.erase(it->second->playHandle);
            }
            m_streams.erase(it);
            LOGI("VideoService") << "Disconnected stream ID " << id;
        }
    }

} // namespace Service
} // namespace Shin
