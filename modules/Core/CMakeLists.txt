# Define the Core static library
add_library(Core STATIC)
set_target_properties(Core PROPERTIES FOLDER "Modules/Core")
add_library(Shin::Core ALIAS Core)

# Add Core submodules
add_subdirectory(Log)