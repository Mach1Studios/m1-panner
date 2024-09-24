juce_add_modules(juce_murka)

# Mach1 Spatial SDK
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/Modules/m1-sdk/cmake) # Prepend the global CMake modules directory
add_subdirectory(m1-sdk/libmach1spatial)
