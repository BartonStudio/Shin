add_library(HCSDK SHARED IMPORTED GLOBAL)

set(HCSDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Import the main HCNetSDK.lib and PlayCtrl.lib
set_target_properties(HCSDK PROPERTIES
    IMPORTED_LOCATION "${HCSDK_ROOT}/bin/HCNetSDK.dll"
    IMPORTED_IMPLIB "${HCSDK_ROOT}/lib/HCNetSDK.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${HCSDK_ROOT}/include"
    INTERFACE_LINK_LIBRARIES "${HCSDK_ROOT}/lib/HCCore.lib;${HCSDK_ROOT}/lib/PlayCtrl.lib"
)

add_library(Shin::ThirdParty::HCSDK ALIAS HCSDK)

