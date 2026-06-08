cmake_minimum_required(VERSION 3.14)
project(Shin VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCV
set(OpenCV_DIR "D:/OpenCV/opencv/build")
find_package(OpenCV 4 REQUIRED)

# Enable solution folders for IDEs (like Visual Studio)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Force all DLLs and EXEs to be output to the same directory
# This ensures that EXEs can find DLLs automatically at runtime!
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Compile third-party dependencies first
add_subdirectory(third_party)

# Add the modules folder, which will manage all sub-modules internally
add_subdirectory(modules)
add_subdirectory(src/Service)

# ==========================================
# Main Application
# ==========================================
add_executable(ShinApp src/main.cpp)
target_link_libraries(ShinApp PRIVATE 
    Shin::Core 
    Shin::UI::Webview
    Service
)
set_target_properties(ShinApp PROPERTIES FOLDER "Application")

# Copy HCSDK DLLs
file(GLOB HCSDK_DLLS "${CMAKE_SOURCE_DIR}/third_party/HCSDK/bin/*.dll")
add_custom_command(TARGET ShinApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HCSDK_DLLS} "$<TARGET_FILE_DIR:ShinApp>"
)

# Copy OpenCV DLLs
file(GLOB OPENCV_DLLS "${OpenCV_DIR}/x64/vc16/bin/*.dll")
add_custom_command(TARGET ShinApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCV_DLLS} "$<TARGET_FILE_DIR:ShinApp>"
)

# Add integration tests
add_subdirectory(test/1-AuthSharedMemoryWebview)