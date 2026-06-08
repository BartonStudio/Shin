# Build Service sources
add_library(Service STATIC 
    VideoService.cpp
)

if(MSVC)
    target_compile_options(Service PRIVATE /FS)
    # Give the PDB a unique name based on the target name
    set_target_properties(Service PROPERTIES COMPILE_PDB_NAME_DEBUG "Service_debug")
endif()

# Link HCSDK, OpenCV, and nlohmann_json
target_link_libraries(Service PUBLIC 
    Shin::ThirdParty::HCSDK 
    ${OpenCV_LIBS} 
    nlohmann_json::nlohmann_json 
    Shin::Core
)

# Include directories
target_include_directories(Service PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/HCSDK/include
)
