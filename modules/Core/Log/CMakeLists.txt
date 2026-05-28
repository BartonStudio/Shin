# Add Log sources and includes to the Core library
target_sources(Core PRIVATE src/Log.cpp)

# Include directories: 
target_include_directories(Core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link spdlog (PRIVATE to hide dependency from consumers)
if(TARGET spdlog::spdlog)
    target_link_libraries(Core PRIVATE spdlog::spdlog)
else()
    target_link_libraries(Core PRIVATE spdlog)
endif()

# Test executable for this module
add_executable(LogTest test/main.cpp)
target_link_libraries(LogTest PRIVATE Shin::Core)
set_target_properties(LogTest PROPERTIES FOLDER "Tests")