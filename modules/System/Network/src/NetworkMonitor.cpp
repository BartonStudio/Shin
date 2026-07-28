#include "NetworkMonitor.hpp"

#include <winsock2.h>   // 必须在 windows.h 之前，避免 winsock 冲突
#include <windows.h>
#include <wlanapi.h>
#include <iphlpapi.h>

#include <vector>

#pragma comment(lib, "wlanapi")
#pragma comment(lib, "iphlpapi")

namespace Shin {
namespace System {

namespace {

    // ---- WLAN 客户端句柄 RAII 封装，确保异常/提前返回时也能关闭 ----
    class WlanHandle {
    public:
        WlanHandle() {
            DWORD negotiatedVersion = 0;
            // 版本 2 适配 Vista 及以上
            if (WlanOpenHandle(2, nullptr, &negotiatedVersion, &m_handle) != ERROR_SUCCESS) {
                m_handle = nullptr;
            }
        }
        ~WlanHandle() {
            if (m_handle) {
                WlanCloseHandle(m_handle, nullptr);
            }
        }
        WlanHandle(const WlanHandle&) = delete;
        WlanHandle& operator=(const WlanHandle&) = delete;

        bool valid() const { return m_handle != nullptr; }
        HANDLE get() const { return m_handle; }

    private:
        HANDLE m_handle = nullptr;
    };

    // 把 quality(0~100) 映射为 0~5 格
    // int QualityToBars(int quality) { ... } // 已移除，前端自行处理

    /**
     * @brief 尝试读取当前已连接的 Wifi 信息。
     * @return true 表示存在一个处于“已连接”状态的无线接口，并已填充 outStatus。
     */
    bool TryQueryWifi(NetworkStatus& outStatus) {
        WlanHandle client;
        if (!client.valid()) {
            return false;
        }

        PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
        if (WlanEnumInterfaces(client.get(), nullptr, &pIfList) != ERROR_SUCCESS || !pIfList) {
            return false;
        }

        bool found = false;
        for (DWORD i = 0; i < pIfList->dwNumberOfItems && !found; ++i) {
            const WLAN_INTERFACE_INFO& ifInfo = pIfList->InterfaceInfo[i];
            if (ifInfo.isState != wlan_interface_state_connected) {
                continue;
            }

            PWLAN_CONNECTION_ATTRIBUTES pConn = nullptr;
            DWORD dataSize = 0;
            DWORD ret = WlanQueryInterface(
                client.get(),
                &ifInfo.InterfaceGuid,
                wlan_intf_opcode_current_connection,
                nullptr,
                &dataSize,
                reinterpret_cast<PVOID*>(&pConn),
                nullptr);

            if (ret == ERROR_SUCCESS && pConn) {
                if (pConn->isState == wlan_interface_state_connected) {
                    const WLAN_ASSOCIATION_ATTRIBUTES& assoc = pConn->wlanAssociationAttributes;

                    outStatus.type = ConnectionType::Wifi;

                    // SSID：ucSSID 为字节数组，长度 uSSIDLength
                    const DOT11_SSID& ssid = assoc.dot11Ssid;
                    outStatus.ssid.assign(
                        reinterpret_cast<const char*>(ssid.ucSSID),
                        static_cast<size_t>(ssid.uSSIDLength));

                    int quality = static_cast<int>(assoc.wlanSignalQuality); // 0~100
                    outStatus.signalQuality = quality;
                    // outStatus.signalDbm  = quality / 2 - 100; // 近似 dBm，暂不使用
                    // outStatus.rxRateKbps = assoc.ulRxRate;    // 链路协商速率，非实时流量，暂不使用
                    // outStatus.txRateKbps = assoc.ulTxRate;    // 链路协商速率，非实时流量，暂不使用

                    found = true;
                }
                WlanFreeMemory(pConn);
            }
        }

        WlanFreeMemory(pIfList);
        return found;
    }

    /**
     * @brief 是否存在一个“已启用且已连接”的以太网接口。
     */
    bool HasActiveEthernet() {
        ULONG family = AF_UNSPEC;
        ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

        ULONG bufLen = 15 * 1024; // 推荐的初始缓冲区大小
        std::vector<unsigned char> buffer(bufLen);

        ULONG ret = GetAdaptersAddresses(
            family, flags, nullptr,
            reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &bufLen);

        if (ret == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(bufLen);
            ret = GetAdaptersAddresses(
                family, flags, nullptr,
                reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &bufLen);
        }

        if (ret != ERROR_SUCCESS) {
            return false;
        }

        for (PIP_ADAPTER_ADDRESSES adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
             adapter != nullptr; adapter = adapter->Next) {
            // 只认真正的有线以太网、已启用、并且拿到了单播 IP 的接口
            if (adapter->IfType == IF_TYPE_ETHERNET_CSMACD &&
                adapter->OperStatus == IfOperStatusUp &&
                adapter->FirstUnicastAddress != nullptr) {
                return true;
            }
        }
        return false;
    }

} // anonymous namespace

    NetworkStatus NetworkMonitor::GetStatus() {
        NetworkStatus status; // 默认 None

        // 优先级：Wifi 已连接 > 以太网在用 > None
        if (TryQueryWifi(status)) {
            return status;
        }

        if (HasActiveEthernet()) {
            status.type = ConnectionType::Ethernet;
            return status;
        }

        return status; // None
    }

    const char* NetworkMonitor::TypeToString(ConnectionType type) {
        switch (type) {
            case ConnectionType::Ethernet: return "Ethernet";
            case ConnectionType::Wifi:     return "Wifi";
            case ConnectionType::None:
            default:                       return "None";
        }
    }

} // namespace System
} // namespace Shin
