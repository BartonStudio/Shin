#pragma once

#include <string>

#ifdef SHIN_SYSTEM_EXPORTS
    #define SHIN_SYSTEM_API __declspec(dllexport)
#else
    #define SHIN_SYSTEM_API __declspec(dllimport)
#endif

namespace Shin {
namespace System {

    /**
     * @brief 当前主网络连接的类型。
     */
    enum class ConnectionType {
        None,       // 无可用连接
        Ethernet,   // 有线（网线）
        Wifi        // 无线
    };

    /**
     * @brief 网络连接状态快照。
     * 仅当 type == Wifi 时，下面的 Wifi 相关字段才有意义。
     */
    struct NetworkStatus {
        ConnectionType type = ConnectionType::None;

        // ---- 仅 Wifi 时有效 ----
        std::string   ssid;                 // 接入的无线网络名称
        int           signalQuality = -1;   // 信号质量 0~100（Windows 原生口径），非 Wifi 为 -1
        // int        signalDbm     = 0;    // 由 quality 换算的近似 dBm（暂不使用）
        // unsigned long rxRateKbps = 0;    // 链路协商速率(Kbps)，非实时流量，暂不使用
        // unsigned long txRateKbps = 0;    // 链路协商速率(Kbps)，非实时流量，暂不使用
    };

    /**
     * @brief 网络状态查询器（按需拉取，无后台线程）。
     *
     * 判定优先级：Wifi 已连接 → Wifi；否则有以太网在用 → Ethernet；都没有 → None。
     * 平台：Windows-only（依赖 wlanapi / iphlpapi）。
     */
    class SHIN_SYSTEM_API NetworkMonitor {
    public:
        /**
         * @brief 同步查询一次当前主网络连接状态。
         */
        static NetworkStatus GetStatus();

        /**
         * @brief 便捷函数：把 ConnectionType 转成可读字符串（"None"/"Ethernet"/"Wifi"）。
         */
        static const char* TypeToString(ConnectionType type);
    };

} // namespace System
} // namespace Shin
