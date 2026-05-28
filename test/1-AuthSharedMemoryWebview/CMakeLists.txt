add_executable(Test_AuthSharedMemoryWebview main.cpp)

# Link both the Webview module (for UI) and the Auth module (for Windows Hello)
target_link_libraries(Test_AuthSharedMemoryWebview PRIVATE 
    Shin::UI::Webview 
    Shin::System::Auth
    Shin::Core
    nlohmann_json::nlohmann_json
)

set_target_properties(Test_AuthSharedMemoryWebview PROPERTIES 
    FOLDER "IntegrationTests"
)

# Automatically copy test HTML next to the generated executable
add_custom_command(TARGET Test_AuthSharedMemoryWebview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/AuthTest.html"
        "$<TARGET_FILE_DIR:Test_AuthSharedMemoryWebview>/AuthTest.html"
)
