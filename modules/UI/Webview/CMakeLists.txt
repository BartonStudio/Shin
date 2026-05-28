add_library(UIWebview SHARED 
    src/WebviewWrapper.cpp 
    src/WebviewMessageHandler.cpp
)
set_target_properties(UIWebview PROPERTIES FOLDER "Modules/UI")

target_compile_definitions(UIWebview PRIVATE SHIN_UIWEBVIEW_EXPORTS)
target_compile_features(UIWebview PUBLIC cxx_std_17)

target_include_directories(UIWebview PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Statically link the configured webview::core_static and nlohmann_json and Shin::Core
target_link_libraries(UIWebview PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(UIWebview PRIVATE webview::core_static Shin::Core)
if(WIN32)
    target_include_directories(UIWebview PRIVATE "${CMAKE_BINARY_DIR}/_deps/microsoft_web_webview2-src/build/native/include")
endif()

# Hide symbols by default (to prevent exporting webview internal symbols)
set_target_properties(UIWebview PROPERTIES 
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

add_library(Shin::UI::Webview ALIAS UIWebview)

# ==========================================
# Test executable for Webview module
# ==========================================

# 1. MultiThreadEventTest
add_executable(MultiThreadEventTest test/MultiThreadEventTest.cpp)
target_link_libraries(MultiThreadEventTest PRIVATE Shin::UI::Webview Shin::Core)
set_target_properties(MultiThreadEventTest PROPERTIES 
    FOLDER "Tests"
)

# Automatically copy test HTML next to the generated executable
add_custom_command(TARGET MultiThreadEventTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/test/MultiThreadEventTest.html"
        "$<TARGET_FILE_DIR:MultiThreadEventTest>/MultiThreadEventTest.html"
)

# 2. WebviewSharedBufferTest
add_executable(WebviewSharedBufferTest test/WebviewSharedBufferTest.cpp)
target_link_libraries(WebviewSharedBufferTest PRIVATE Shin::UI::Webview Shin::Core)
if(WIN32)
    target_include_directories(WebviewSharedBufferTest PRIVATE "${CMAKE_BINARY_DIR}/_deps/microsoft_web_webview2-src/build/native/include")
endif()
set_target_properties(WebviewSharedBufferTest PROPERTIES 
    FOLDER "Tests"
)

# Automatically copy test HTML next to the generated executable
add_custom_command(TARGET WebviewSharedBufferTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/test/WebviewSharedBufferTest.html"
        "$<TARGET_FILE_DIR:WebviewSharedBufferTest>/WebviewSharedBufferTest.html"
)

# 3. SharedMemoryManagerTest
add_executable(SharedMemoryManagerTest test/SharedMemoryManagerTest.cpp)
target_link_libraries(SharedMemoryManagerTest PRIVATE Shin::UI::Webview Shin::Core)
set_target_properties(SharedMemoryManagerTest PROPERTIES 
    FOLDER "Tests"
)

# Automatically copy test HTML next to the generated executable
add_custom_command(TARGET SharedMemoryManagerTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/test/SharedMemoryManagerTest.html"
        "$<TARGET_FILE_DIR:SharedMemoryManagerTest>/SharedMemoryManagerTest.html"
)
