# Add Config sources and includes to the Core library
target_sources(Core PRIVATE src/Config.cpp)

# Include directories: 
target_include_directories(Core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Config Test
add_executable(ConfigTest test/ConfigTest.cpp)
target_link_libraries(ConfigTest PRIVATE Shin::Core)
set_target_properties(ConfigTest PROPERTIES FOLDER "Tests")
