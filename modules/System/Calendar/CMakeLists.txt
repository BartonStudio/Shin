add_library(Calendar SHARED 
    src/MemoManager.cpp
)
set_target_properties(Calendar PROPERTIES 
    FOLDER "Modules/System"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/Debug"
)

target_compile_definitions(Calendar PRIVATE SHIN_CALENDAR_EXPORTS)
target_compile_features(Calendar PUBLIC cxx_std_17)

target_include_directories(Calendar PUBLIC 
    include 
    "${CMAKE_SOURCE_DIR}/third_party/SQLiteCpp/include"
    "${CMAKE_SOURCE_DIR}/third_party/json/include"
)

target_link_libraries(Calendar PRIVATE Shin::Core SQLiteCpp)

# Create an ALIAS target
add_library(Shin::Calendar ALIAS Calendar)
