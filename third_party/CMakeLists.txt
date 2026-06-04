# Build third-party dependencies

# Group all targets created in this directory (and subdirectories) into the "ThirdParty" folder in IDEs
set(CMAKE_FOLDER "ThirdParty")

# Ensure static libraries are built with Position Independent Code (PIC)
# so they can be linked into shared libraries.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Force spdlog to be built as a static library (not header-only, not shared)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as a shared library" FORCE)
set(SPDLOG_COMPILED ON CACHE BOOL "Build spdlog as a compiled library" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/CMakeLists.txt")
    add_subdirectory(spdlog)
else()
    message(WARNING "spdlog submodule not found in third_party. Please initialize submodules.")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/webview/CMakeLists.txt")
    # Configure webview to build only the static library
    set(WEBVIEW_BUILD_STATIC_LIBRARY ON CACHE BOOL "Build webview static library" FORCE)
    set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "Build webview shared library" FORCE)
    set(WEBVIEW_BUILD_TESTS OFF CACHE BOOL "Build webview tests" FORCE)
    set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "Build webview examples" FORCE)
    
    # Update WebView2 SDK version to support ICoreWebView2Environment12 (SharedBuffer)
    set(WEBVIEW_MSWEBVIEW2_VERSION "1.0.2592.51" CACHE STRING "MS WebView2 version" FORCE)

    add_subdirectory(webview)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/json/CMakeLists.txt")
    set(JSON_BuildTests OFF CACHE BOOL "Build nlohmann/json tests" FORCE)
    add_subdirectory(json)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toml11/CMakeLists.txt")
    set(TOML11_BUILD_TESTS OFF CACHE BOOL "Build toml11 tests" FORCE)
    set(TOML11_BUILD_EXAMPLES OFF CACHE BOOL "Build toml11 examples" FORCE)
    add_subdirectory(toml11)
endif()