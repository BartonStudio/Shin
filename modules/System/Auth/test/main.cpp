#include "LocalAuthenticator.hpp"
#include <iostream>
#include <windows.h>

int main() {
    std::wcout << L"=== Windows Local Authenticator Test ===" << std::endl;

    std::wcout << L"Checking availability..." << std::endl;
    bool isAvailable = Shin::System::LocalAuthenticator::IsAvailable();
    if (!isAvailable) {
        std::wcout << L"[INFO] Local authentication is not available or not configured." << std::endl;
    } else {
        std::wcout << L"[INFO] Local authentication is ready!" << std::endl;
    }

    Shin::System::AuthOptions options;
    options.promptMessage = L"Shin Module Test: Please verify your identity to continue.";
    options.allowBypassIfUnconfigured = true; 
    options.parentWindowHandle = nullptr;     

    std::wcout << L"\nPrompting for authentication..." << std::endl;

    auto result = Shin::System::LocalAuthenticator::VerifyUser(options);

    std::wcout << L"\n>>> Result: ";
    switch (result) {
        case Shin::System::AuthResult::Success:
            std::wcout << L"Success!" << std::endl;
            break;
        case Shin::System::AuthResult::Bypassed:
            std::wcout << L"Bypassed (No password/PIN configured)." << std::endl;
            break;
        case Shin::System::AuthResult::Canceled:
            std::wcout << L"Canceled by user." << std::endl;
            break;
        case Shin::System::AuthResult::Failed:
            std::wcout << L"Authentication failed." << std::endl;
            break;
        case Shin::System::AuthResult::Unavailable:
            std::wcout << L"Unavailable (System policy or exception)." << std::endl;
            break;
        case Shin::System::AuthResult::Error:
            std::wcout << L"Unknown Error occurred." << std::endl;
            break;
    }

    std::wcout << L"\nPress ENTER to exit..." << std::endl;
    std::cin.get();

    return 0;
}