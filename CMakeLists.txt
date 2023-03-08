cmake_minimum_required(VERSION 3.15.0)
file(STRINGS VERSION CURRENT_VERSION)
project(M1-Panner VERSION ${CURRENT_VERSION})

### IDE Generator pre-config ###
# Xcode: Disable automatic build scheme generation globally.
#        Instead, we explicitely specify which targets require schemes.
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
# Enable to build universal binaries on macOS, increasing build time
# This only affects local builds, GitHub actions always builds Universals
set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Support macOS down to High Sierra")
# IDEs:  Enable grouping of source files into folders in IDEs.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# IDEs:  Create a folder in the IDE with the JUCE Module code.
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

# add JUCE
add_subdirectory(JUCE)
# add custom modules
add_subdirectory(Modules)

if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/JUCE_deps)
    set(AAX_PATH "JUCE_deps/AAX_SDK_2p3p2")
    set(VST2_PATH "JUCE_deps/VST_SDK_vst2/VST2_SDK")
    message(STATUS "JUCE Dependencies Found: ${PROJECT_SOURCE_DIR}/JUCE_deps")
else()
    message(STATUS "Warning: Did not find additional JUCE deps submodule. Please add the appropriate SDKs manually.")
endif()

option(ITD_PARAMETERS "Adds the experimental ITD processing parameters and UI" OFF)
option(BUILD_VST3 "Compile VST3 plugin type" OFF)
option(BUILD_VST "Compile VST2 plugin type" OFF)
option(BUILD_AAX "Compile AAX plugin type" OFF)
option(BUILD_AU "Compile AU plugin type" OFF)
option(BUILD_AUV3 "Compile AUv3 plugin type" OFF)
option(BUILD_UNITY "Compile Unity plugin type" OFF)
option(BUILD_STANDALONE "Compile Standalone app of plugin" OFF)

# These are used to re-apply brackets for the JucePlugin_PreferredChannelConfigurations
set(LBRACKET_LITERAL "{")
set(RBRACKET_LITERAL "}")

# DYNAMIC_IO_PLUGIN_MODE or STREAMING_PANNER_PLUGIN or AAX/RTAS
set(PLUGIN_NAME "${CMAKE_PROJECT_NAME}")
set(CUSTOM_BUNDLE_ID "com.mach1.panner")
set(CUSTOM_PLUGIN_CODE "M1PN")

if(ITD_PARAMETERS)
    add_definitions(-DITD_PARAMETERS)
    message(STATUS "Compiling with experimental ITD parameters.")
endif(ITD_PARAMETERS)

# check which formats we want to build
# Valid formats: AAX VST VST3 AU // Not yet supported: Unity AUv3 Standalone CLAP
# Will only allow either AAX or other plugin formats, not both
if(BUILD_AAX AND AAX_PATH)
    list(APPEND FORMATS "AAX")
    juce_set_aax_sdk_path(${AAX_PATH})
    message(STATUS "AAX SDK path set as: ${AAX_PATH}")
endif()

if(BUILD_VST3)
    list(APPEND FORMATS "VST3")
endif(BUILD_VST3)

if(BUILD_VST AND VST2_PATH)
    list(APPEND FORMATS "VST")
    juce_set_vst2_sdk_path(${VST2_PATH})
    message(STATUS "VST2 SDK path set as: ${VST2_PATH}")
endif()

if(BUILD_UNITY)
    list(APPEND FORMATS "Unity")
endif()

if(BUILD_STANDALONE)
    list(APPEND FORMATS "Standalone")
endif()

if(BUILD_AU AND APPLE)
    list(APPEND FORMATS "AU")
endif()

if(BUILD_AUV3 AND APPLE)
    list(APPEND FORMATS "AUv3")
endif()

message(STATUS "Plugin targets: ${FORMATS}")

# add the plugin targets
juce_add_plugin(${PLUGIN_NAME}
                VERSION ${CURRENT_VERSION}
                COMPANY_NAME "Mach1"
                COMPANY_WEBSITE "https://mach1.tech"
                COMPANY_EMAIL "whatsup@mach1.tech"
                PLUGIN_MANUFACTURER_CODE "Mac1"
                PLUGIN_CODE ${CUSTOM_PLUGIN_CODE}
                FORMATS ${FORMATS}
                VST3_CATEGORIES "Spatial Surround Up-Downmix"
                AAX_CATEGORY "AAX_ePlugInCategory_None"
                AU_MAIN_TYPE "kAudioUnitType_Effect"
                BUNDLE_ID ${CUSTOM_BUNDLE_ID}
                PLUGIN_NAME ${PLUGIN_NAME}
                PRODUCT_NAME ${PLUGIN_NAME}
                IS_SYNTH FALSE
                COPY_PLUGIN_AFTER_BUILD TRUE)

target_compile_definitions(${PLUGIN_NAME}
    PUBLIC 
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Set the C++ language standard requirenment for the "shared code" library target.
# Setting this to PUBLIC ensures that all dependent targets will inherit the specified C++ standard.
target_compile_features("${PLUGIN_NAME}" PUBLIC cxx_std_17)
# Disable compiler extensions for the project targets (e.g. use -std=c++17 instead of -std=gnu++17).
get_property(project_targets DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
set_target_properties(${project_targets} PROPERTIES CXX_EXTENSIONS OFF)

# add the sources
add_subdirectory(Source)
set_target_properties("${PLUGIN_NAME}" PROPERTIES FOLDER "")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "" FILES ${SourceFiles})

add_subdirectory(Resources)

# Required for Linux happiness:
# See https://forum.juce.com/t/loading-pytorch-model-using-binarydata/39997/2
set_target_properties(Resources PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# This cleans up the folder organization on Xcode.
# It tucks the Plugin varieties into a "Targets" folder and generate an Xcode Scheme manually
# Xcode scheme generation is turned off globally to limit noise from other targets
# The non-hacky way of doing this is via the global PREDEFINED_TARGETS_FOLDER property
# However that doesn't seem to be working in Xcode
# Not all plugin types (au, vst) available on each build type (win, macos, linux)

foreach(target ${FORMATS} "All")
    if(TARGET ${PLUGIN_NAME}_${target})
        if (${target} STREQUAL "VST" OR ${target} STREQUAL "VST3")
                set_target_properties(${PLUGIN_NAME}_${target} PROPERTIES
                # Tuck the actual plugin targets into a folder where they won't bother us
                FOLDER "Targets"
                
                # MacOS only: Sets the default executable that Xcode will open on build
                # For this exact path to to work, manually build the AudioPluginHost.xcodeproj in the JUCE subdir
                #XCODE_SCHEME_EXECUTABLE ${PROJECT_SOURCE_DIR}/JUCE/extras/AudioPluginHost/Builds/MacOSX/build/Debug/AudioPluginHost.app
                XCODE_SCHEME_EXECUTABLE /Applications/REAPER.app

                # Let us build the target in Xcode
                XCODE_GENERATE_SCHEME ON)
        else()
            set_target_properties(${PLUGIN_NAME}_${target} PROPERTIES
                # Tuck the actual plugin targets into a folder where they won't bother us
                FOLDER "Targets"
                
                # MacOS only: Sets the default executable that Xcode will open on build
                # For this exact path to to work, manually build the AudioPluginHost.xcodeproj in the JUCE subdir
                #XCODE_SCHEME_EXECUTABLE ${PROJECT_SOURCE_DIR}/JUCE/extras/AudioPluginHost/Builds/MacOSX/build/Debug/AudioPluginHost.app

                # Let us build the target in Xcode
                XCODE_GENERATE_SCHEME ON)
        endif()
    endif()
endforeach()
set_target_properties(Resources PROPERTIES FOLDER "Targets")

# add required flags
juce_generate_juce_header(${PLUGIN_NAME})

target_link_libraries(${PLUGIN_NAME} PRIVATE juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats juce::juce_audio_plugin_client juce::juce_audio_processors juce::juce_audio_utils juce::juce_core juce::juce_cryptography juce::juce_data_structures juce::juce_dsp juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra juce::juce_opengl juce::juce_osc juce::juce_product_unlocking juce_murka)
target_link_libraries(${PLUGIN_NAME} PRIVATE Resources PUBLIC juce::juce_recommended_warning_flags juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# definitions to replace the `JucePluginDefines.h`
set(JucePlugin_Name ${PLUGIN_NAME})
set(JucePlugin_Desc ${PLUGIN_NAME})

# add M1-SDK
set(MACH1SPATIAL_LIBS_PATH "Modules/m1-sdk/mach1spatial-libs")
if(APPLE)
    set(MACH1SPATIAL_LIBS_UNIX_PATH ${MACH1SPATIAL_LIBS_PATH}/xcode)
else()
    set(MACH1SPATIAL_LIBS_UNIX_PATH ${MACH1SPATIAL_LIBS_PATH}/linux)
endif()

# link libraries
if(WIN32 OR MSVC OR MINGW)
    find_library(MACH1ENCODE_LIBRARY_RELEASE
        NAMES Mach1EncodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Release
    )
    find_library(MACH1ENCODE_LIBRARY_DEBUG
        NAMES Mach1EncodeCAPId
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Debug
    )
    SET(MACH1ENCODE_LIBRARY
        debug ${MACH1ENCODE_LIBRARY_DEBUG}
        optimized ${MACH1ENCODE_LIBRARY_RELEASE}
    )
else()
    find_library(MACH1ENCODE_LIBRARY 
        NAMES Mach1EncodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_UNIX_PATH}/lib
    )
endif()

set(MACH1SPATIAL_INCLUDES ${MACH1SPATIAL_LIBS_PATH}/linux/include ${MACH1SPATIAL_LIBS_PATH}/linux/include/M1DSP ${MACH1SPATIAL_LIBS_PATH}/xcode/include ${MACH1SPATIAL_LIBS_PATH}/xcode/include/M1DSP ${MACH1SPATIAL_LIBS_PATH}/windows-x86_64/include ${MACH1SPATIAL_LIBS_PATH}/windows-x86_64/include/M1DSP)
include_directories(${MACH1SPATIAL_INCLUDES})
target_link_libraries(${PLUGIN_NAME} PRIVATE ${MACH1ENCODE_LIBRARY})
if(WIN32)
    add_compile_definitions(M1_STATIC)
    message(STATUS "Adding windows OS flags")
endif()
target_sources(${PLUGIN_NAME} PRIVATE ${MACH1SPATIAL_LIBS_PATH}/xcode/include/Mach1Encode.cpp)

### IDE Generator post-config ###
# IDEs:  Move the "shared code" source group (main target) out of the "Targets" folder
#        to the top level.
set_target_properties("${PLUGIN_NAME}" PROPERTIES FOLDER "")

# IDEs:  Organise source file grouping.
get_target_property(project_sources "${PLUGIN_NAME}" SOURCES)

#   If JuceHeader.h is generated, remove it from the source file list and handle it individually.
get_target_property(juce_library_code "${PLUGIN_NAME}" JUCE_GENERATED_SOURCES_DIRECTORY)
set(juce_header "${juce_library_code}/JuceHeader.h")
list(REMOVE_ITEM project_sources "${juce_header}")
# remove m1-sdk cpp files too
list(REMOVE_ITEM project_sources "${MACH1SPATIAL_LIBS_PATH}/xcode/include/Mach1Encode.cpp")

#   Place JuceHeader.h and the associated CMake Rule file into a "JUCE Library Code" folder.
source_group("JUCE Library Code" FILES "${juce_header}")
source_group("JUCE Library Code/CMake Rules" FILES "${juce_header}.rule")

#   Generate source groups that follow the organisation of source file directories.
source_group(TREE "${PROJECT_SOURCE_DIR}/Source" FILES ${project_sources})

#   Place Binary Data related source files into the root of their target folder.
if(TARGET Resources)
    get_target_property(project_assets Resources SOURCES)
    source_group("" FILES ${project_assets})
endif()

# Color our warnings and errors
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options(-fcolor-diagnostics)
endif()
