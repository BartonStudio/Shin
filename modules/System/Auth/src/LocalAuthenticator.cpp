#include "LocalAuthenticator.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.UI.h>
#include <UserConsentVerifierInterop.h>

#pragma comment(lib, "windowsapp")

using namespace winrt;
using namespace Windows::Security::Credentials::UI;

namespace Shin {
namespace System {

    static void EnsureWinRTInitialized() {
        try {
            winrt::init_apartment();
        } catch (const winrt::hresult_error&) {
        }
    }

    bool LocalAuthenticator::IsAvailable() {
        EnsureWinRTInitialized();
        try {
            auto availability = UserConsentVerifier::CheckAvailabilityAsync().get();
            return availability == UserConsentVerifierAvailability::Available || 
                   availability == UserConsentVerifierAvailability::NotConfiguredForUser;
        } catch (...) {
            return false;
        }
    }

    AuthResult LocalAuthenticator::VerifyUser(const AuthOptions& options) {
        EnsureWinRTInitialized();
        try {
            auto availability = UserConsentVerifier::CheckAvailabilityAsync().get();
            
            if (availability == UserConsentVerifierAvailability::NotConfiguredForUser) {
                if (options.allowBypassIfUnconfigured) {
                    return AuthResult::Bypassed;
                } else {
                    return AuthResult::Unavailable;
                }
            } else if (availability != UserConsentVerifierAvailability::Available) {
                return AuthResult::Unavailable;
            }

            HWND hwnd = options.parentWindowHandle;

            auto factory = get_activation_factory<UserConsentVerifier, IUserConsentVerifierInterop>();
            winrt::Windows::Foundation::IAsyncOperation<UserConsentVerificationResult> asyncOp{ nullptr };

            if (hwnd) EnableWindow(hwnd, FALSE);

            winrt::check_hresult(factory->RequestVerificationForWindowAsync(
                hwnd ? hwnd : GetConsoleWindow(), 
                static_cast<HSTRING>(winrt::get_abi(winrt::hstring(options.promptMessage))),
                winrt::guid_of<winrt::Windows::Foundation::IAsyncOperation<UserConsentVerificationResult>>(),
                winrt::put_abi(asyncOp)
            ));

            auto result = asyncOp.get();

            if (hwnd) {
                EnableWindow(hwnd, TRUE);
                SetForegroundWindow(hwnd);
            }

            if (result == UserConsentVerificationResult::Verified) {
                return AuthResult::Success;
            } else if (result == UserConsentVerificationResult::Canceled) {
                return AuthResult::Canceled;
            } else {
                return AuthResult::Failed;
            }

        } catch (...) {
            return AuthResult::Error;
        }
    }

}
}