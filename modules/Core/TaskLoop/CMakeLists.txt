# Add TaskLoop sources and includes to the Core library
target_sources(Core PRIVATE src/TaskLoop.cpp)

# Include directories:
target_include_directories(Core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executable for this module
add_executable(TaskLoopTest test/TaskLoopTest.cpp)
target_link_libraries(TaskLoopTest PRIVATE Shin::Core)
set_target_properties(TaskLoopTest PROPERTIES FOLDER "Tests")
