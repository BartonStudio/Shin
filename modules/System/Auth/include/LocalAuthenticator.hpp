#pragma once

#include <string>
#include <windows.h>

#ifdef SHIN_SYSTEM_EXPORTS
    #define SHIN_SYSTEM_API __declspec(dllexport)
#else
    #define SHIN_SYSTEM_API __declspec(dllimport)
#endif

namespace Shin {
namespace System {

    enum class AuthResult {
        Success,
        Bypassed,
        Canceled,
        Failed,
        Unavailable,
        Error
    };

    struct AuthOptions {
        std::wstring promptMessage = L"Please verify your identity to continue."; 
        HWND parentWindowHandle = nullptr; 
        bool allowBypassIfUnconfigured = true; 
    };

    class SHIN_SYSTEM_API LocalAuthenticator {
    public:
        static bool IsAvailable();
        static AuthResult VerifyUser(const AuthOptions& options = AuthOptions());
    };

}
}