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
# 2. SharedMemory Module
# ==========================================
add_library(SharedMemory SHARED SharedMemory/src/SharedMemory.cpp)
set_target_properties(SharedMemory PROPERTIES FOLDER "Modules/System")

target_compile_definitions(SharedMemory PRIVATE SHIN_SYSTEM_EXPORTS)
target_compile_features(SharedMemory PUBLIC cxx_std_17)

target_include_directories(SharedMemory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/SharedMemory/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(SharedMemory PRIVATE windowsapp Shin::Core)

# Create an ALIAS target
add_library(Shin::System::SharedMemory ALIAS SharedMemory)

# SystemSharedMemory Test
add_executable(SystemSharedMemoryTest SharedMemory/test/SystemSharedMemoryTest.cpp)
target_link_libraries(SystemSharedMemoryTest PRIVATE Shin::System::SharedMemory Shin::Core)
set_target_properties(SystemSharedMemoryTest PROPERTIES FOLDER "Tests")

# ==========================================
# Node.js Bindings (Native CMake Integration)
# ==========================================
option(SHIN_BUILD_NODE_BINDINGS "Build Node.js bindings" ON)

if(SHIN_BUILD_NODE_BINDINGS)
    find_program(NODE_EXECUTABLE node)
    find_program(NPM_EXECUTABLE NAMES npm.cmd npm)

    if(NODE_EXECUTABLE AND NPM_EXECUTABLE)
        message(STATUS "Node.js found. Configuring native Node Addon build...")
        
        # 1. 获取 Node 版本和架构
        execute_process(COMMAND ${NODE_EXECUTABLE} -p "process.version" OUTPUT_VARIABLE NODE_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${NODE_EXECUTABLE} -p "process.arch" OUTPUT_VARIABLE NODE_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
        
        # 架构映射 (ia32 -> x86, x64 -> x64, arm64 -> arm64)
        set(NODE_LIB_ARCH ${NODE_ARCH})
        if(NODE_ARCH STREQUAL "ia32")
            set(NODE_LIB_ARCH "x86")
        endif()

        # 2. 准备缓存目录
        set(NODE_CACHE_DIR "${CMAKE_BINARY_DIR}/node_cache/${NODE_VERSION}")
        set(NODE_LIB_FILE "${NODE_CACHE_DIR}/win-${NODE_LIB_ARCH}/node.lib")
        set(NODE_HEADERS_DIR "${NODE_CACHE_DIR}/include/node")

        # 3. 自动下载 node.lib (仅限 Windows)
        if(WIN32 AND NOT EXISTS "${NODE_LIB_FILE}")
            message(STATUS "Downloading node.lib ${NODE_VERSION} for ${NODE_LIB_ARCH}...")
            file(DOWNLOAD "https://nodejs.org/dist/${NODE_VERSION}/win-${NODE_LIB_ARCH}/node.lib" "${NODE_LIB_FILE}" SHOW_PROGRESS)
        endif()

        # 4. 自动下载 Node 头文件
        if(NOT EXISTS "${NODE_HEADERS_DIR}/node.h")
            message(STATUS "Downloading Node headers ${NODE_VERSION}...")
            set(NODE_TAR_FILE "${NODE_CACHE_DIR}/node-headers.tar.gz")
            file(DOWNLOAD "https://nodejs.org/dist/${NODE_VERSION}/node-${NODE_VERSION}-headers.tar.gz" "${NODE_TAR_FILE}" SHOW_PROGRESS)
            execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${NODE_TAR_FILE}" WORKING_DIRECTORY "${NODE_CACHE_DIR}")
            file(MAKE_DIRECTORY "${NODE_CACHE_DIR}/include")
            file(RENAME "${NODE_CACHE_DIR}/node-${NODE_VERSION}/include/node" "${NODE_HEADERS_DIR}")
        endif()

        # 5. 获取 node-addon-api
        set(NODE_ADDON_API_DIR "${CMAKE_BINARY_DIR}/node_modules/node-addon-api")
        if(NOT EXISTS "${NODE_ADDON_API_DIR}")
            message(STATUS "Installing node-addon-api in build directory...")
            execute_process(COMMAND ${NPM_EXECUTABLE} init -y WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
            execute_process(COMMAND ${NPM_EXECUTABLE} install node-addon-api WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        endif()

        # 6. 定义 Target
        if(EXISTS "${NODE_HEADERS_DIR}" AND EXISTS "${NODE_ADDON_API_DIR}")
            add_library(SharedMemoryAddon SHARED SharedMemory/src/bindings/node/shm_addon.cpp)
            set_target_properties(SharedMemoryAddon PROPERTIES PREFIX "" SUFFIX ".node" FOLDER "Bindings/Node")
            
            target_include_directories(SharedMemoryAddon PRIVATE 
                ${NODE_HEADERS_DIR}
                ${NODE_ADDON_API_DIR}
            )
            
            target_compile_definitions(SharedMemoryAddon PRIVATE NAPI_VERSION=8 NAPI_CPP_EXCEPTIONS)
            
            if(WIN32)
                target_link_libraries(SharedMemoryAddon PRIVATE ${NODE_LIB_FILE})
            endif()

            # 重点：链接到新的独立核心引擎 (SystemSharedMemory)
            target_link_libraries(SharedMemoryAddon PRIVATE Shin::System::SharedMemory)
            
            message(STATUS "Successfully configured SharedMemoryAddon.node target.")
        endif()
    else()
        message(WARNING "Node.js or npm not found in PATH. Skipping Node bindings.")
    endif()
endif()