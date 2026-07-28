#include "NetworkMonitor.hpp"
#include <iostream>

int main() {
    std::cout << "=== Shin Network Monitor Test ===" << std::endl;

    Shin::System::NetworkStatus status = Shin::System::NetworkMonitor::GetStatus();

    std::cout << "Connection Type : "
              << Shin::System::NetworkMonitor::TypeToString(status.type) << std::endl;

    if (status.type == Shin::System::ConnectionType::Wifi) {
        std::cout << "SSID            : " << status.ssid << std::endl;
        std::cout << "Signal Quality  : " << status.signalQuality << " / 100" << std::endl;
    } else if (status.type == Shin::System::ConnectionType::Ethernet) {
        std::cout << "Wired connection is active." << std::endl;
    } else {
        std::cout << "No active network connection." << std::endl;
    }

    std::cout << "\nPress ENTER to exit..." << std::endl;
    std::cin.get();
    return 0;
}
