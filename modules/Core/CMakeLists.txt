# Define the Core shared library
add_library(Core SHARED)
set_target_properties(Core PROPERTIES FOLDER "Modules/Core")
add_library(Shin::Core ALIAS Core)

# Export macro for the Core library
target_compile_definitions(Core PRIVATE SHIN_CORE_EXPORTS)
target_compile_features(Core PUBLIC cxx_std_17)

# Link third-party libraries
target_link_libraries(Core PUBLIC toml11::toml11 nlohmann_json::nlohmann_json)

# Add Core submodules
add_subdirectory(Log)
add_subdirectory(Config)
add_subdirectory(TaskLoop)

# Add top-level Core sources
target_sources(Core PRIVATE Module.cpp)

# Add top-level Core headers to the target
target_include_directories(Core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Core Tests
add_executable(TomlTest test/TomlTest.cpp)
target_link_libraries(TomlTest PRIVATE Shin::Core)
set_target_properties(TomlTest PROPERTIES FOLDER "Tests")