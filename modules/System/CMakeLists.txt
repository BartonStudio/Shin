# ==========================================
# 1. Auth Module
# ==========================================
add_library(Auth SHARED Auth/src/LocalAuthenticator.cpp)
set_target_properties(Auth PROPERTIES FOLDER "Modules/System")

# 导出宏，配合 include 中的 #ifdef SHIN_SYSTEM_EXPORTS 使用
target_compile_definitions(Auth PRIVATE SHIN_SYSTEM_EXPORTS)
target_compile_features(Auth PUBLIC cxx_std_17)

target_include_directories(Auth PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Auth/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Auth PRIVATE windowsapp Shin::Core)

# Create an ALIAS target
add_library(Shin::System::Auth ALIAS Auth)

# Auth Test
add_executable(AuthTest Auth/test/main.cpp)
target_link_libraries(AuthTest PRIVATE Shin::System::Auth)
set_target_properties(AuthTest PROPERTIES FOLDER "Tests")

# ==========================================
# 2. Calendar Module
# ==========================================
add_library(Calendar SHARED Calendar/src/MemoManager.cpp)
set_target_properties(Calendar PROPERTIES FOLDER "Modules/System")

target_compile_definitions(Calendar PRIVATE SHIN_SYSTEM_EXPORTS)
target_compile_features(Calendar PUBLIC cxx_std_17)

target_include_directories(Calendar PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Calendar/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Calendar PRIVATE Shin::Core SQLiteCpp)

# Create an ALIAS target
add_library(Shin::System::Calendar ALIAS Calendar)

# calendar Test
add_executable(CalendarTest Calendar/test/main.cpp)
target_link_libraries(CalendarTest PRIVATE Shin::System::Calendar)
set_target_properties(CalendarTest PROPERTIES FOLDER "Tests")

# ==========================================
# 3. Network Module
# ==========================================
add_library(Network SHARED Network/src/NetworkMonitor.cpp)
set_target_properties(Network PROPERTIES FOLDER "Modules/System")

target_compile_definitions(Network PRIVATE SHIN_SYSTEM_EXPORTS)
target_compile_features(Network PUBLIC cxx_std_17)
# 源码为 UTF-8（含中文注释），强制 MSVC 按 UTF-8 解析，避免 936 代码页误读
target_compile_options(Network PRIVATE /utf-8)

target_include_directories(Network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Network/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Network PRIVATE Shin::Core wlanapi iphlpapi)

# Create an ALIAS target
add_library(Shin::System::Network ALIAS Network)

# Network Test
add_executable(NetworkTest Network/test/main.cpp)
target_link_libraries(NetworkTest PRIVATE Shin::System::Network)
target_compile_options(NetworkTest PRIVATE /utf-8)
set_target_properties(NetworkTest PROPERTIES FOLDER "Tests")