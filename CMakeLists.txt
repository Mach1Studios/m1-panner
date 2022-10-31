cmake_minimum_required(VERSION 3.13.0)
project(M1-Panner VERSION 2.0.0)
add_subdirectory(JUCE)

if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/JUCE_deps)
    set(AAX_PATH "JUCE_deps/AAX_SDK_2p3p2")
    set(VST2_PATH "JUCE_deps/VST_SDK_vst2/VST2_SDK")
    message("-- JUCE Dependencies Found: ${PROJECT_SOURCE_DIR}/JUCE_deps")
else()
    message("Warning: Did not find additional JUCE deps submodule. Please add the appropriate SDKs manually.")
endif()

OPTION(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT "Compiles a single I/O static setup pugin, requires setting CONFIG_IO for example: `-DINPUTS=1 -DOUTPUTS=8`; n,N where n=number_of_input_channels and N=number_of_output_channels" OFF)
IF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT)
    MESSAGE("Make sure the 'INPUTS' and 'OUTPUTS' are set!")
ENDIF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT)

OPTION(BUILD_STREAMING_PANNER_PLUGIN "Compiles the plugin UI and processing code for external spatial audio processing" OFF) # Disabled by default
OPTION(BUILD_DYNAMIC_IO_PLUGIN_MODE "Compiles the plugin UI and processing code for re-declarable I/O" ON) # Enabled by default
OPTION(BUILD_ITD_PARAMETERS "Adds the experimental ITD processing parameters and UI" OFF) # Disabled by default
OPTION(BUILD_VST3 "Compile VST3 plugin type" ON) # Enabled by default

SET(LBRACKET_LITERAL "{")
SET(RBRACKET_LITERAL "}")

IF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT && (INPUTS GREATER 0 && OUTPUTS GREATER 0))
    SET(STATIC_PANNER_IO "(${INPUTS}-${OUTPUTS})")
    SET(CUSTOM_BUDLE_ID "com.mach1.panner.${INPUTS}.${OUTPUTS}")
    SET(CUSTOM_PLUGIN_CODE "MP${INPUTS}${OUTPUTS}")
    ADD_DEFINITIONS(JucePlugin_PreferredChannelConfig "${LBRACKET_LITERAL}${INPUTS},${OUTPUTS}${RBRACKET_LITERAL}")
    # TODO: Change output name to reflect
ELSEIF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT && !(INPUTS GREATER 0 && OUTPUTS GREATER 0))
    MESSAGE(SEND_ERROR "INPUTS and OUTPUTS are not defined!")
ELSE()
    SET(CUSTOM_BUDLE_ID "com.mach1.panner")
    SET(CUSTOM_PLUGIN_CODE "M1PN")
    IF(BUILD_STREAMING_PANNER_PLUGIN)
        ADD_DEFINITIONS(-DSTREAMING_PANNER_PLUGIN)
    ENDIF(BUILD_STREAMING_PANNER_PLUGIN)

    IF(BUILD_DYNAMIC_IO_PLUGIN_MODE)
        ADD_DEFINITIONS(-DDYNAMIC_IO_PLUGIN_MODE)
    ENDIF(BUILD_DYNAMIC_IO_PLUGIN_MODE)
ENDIF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT)

IF(BUILD_ITD_PARAMETERS)
    ADD_DEFINITIONS(-DITD_PARAMETERS)
ENDIF(BUILD_ITD_PARAMETERS)

# check which formats we want to build
IF(BUILD_VST3)
    list(APPEND FORMATS "VST3")
ENDIF(BUILD_VST3)

if (VST2_PATH)
    list(APPEND FORMATS "VST2")
    juce_set_vst2_sdk_path(${VST2_PATH})
endif()

if (AAX_PATH)
    list(APPEND FORMATS "AAX")
    juce_set_aax_sdk_path(${AAX_PATH})
endif()

if (APPLE)
    list(APPEND FORMATS "AU")
endif()

# add the plugin targets
juce_add_plugin(${CMAKE_PROJECT_NAME}
                VERSION "2.0.0"
                COMPANY_NAME "Mach1"
                PLUGIN_MANUFACTURER_CODE "Mac1"
                PLUGIN_CODE ${CUSTOM_PLUGIN_CODE}
                FORMATS ${FORMATS}
                VST3_CATEGORIES "Spatial Surround Up-Downmix"
                AAX_CATEGORY "AAX_ePlugInCategory_None"
                AU_MAIN_TYPE "kAudioUnitType_Effect"
                COMPANY_WEBSITE "https://mach1.tech"
                COMPANY_EMAIL "whatsup@mach1.tech"
                BUNDLE_ID ${CUSTOM_BUDLE_ID}
                PLUGIN_NAME "${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO}"
                PRODUCT_NAME "${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO}")

# add the sources
add_subdirectory(Resources)
add_subdirectory(Source)

# add required flags
target_link_libraries(${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO} PRIVATE juce::juce_recommended_warning_flags juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
target_link_libraries(${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO} PRIVATE juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats juce::juce_audio_plugin_client juce::juce_audio_processors juce::juce_audio_utils juce::juce_core juce::juce_cryptography juce::juce_data_structures juce::juce_dsp juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra juce::juce_opengl juce::juce_osc juce::juce_product_unlocking juce_murka m1_orientation_client)
target_compile_definitions(${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO}
    PUBLIC 
    JUCE_WEB_BROWSER=0)
juce_generate_juce_header(${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO})

# setup the copying to the output folder
if (APPLE)
    set(COPY_FOLDER ${CMAKE_SOURCE_DIR}/Builds/MacOSX)
elseif(WIN32)
    set(COPY_FOLDER ${CMAKE_SOURCE_DIR}/Builds/VisualStudio2015)
endif()

foreach(FORMAT ${FORMATS})
    get_target_property(ARTEFACTS_DIR ${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO}${FORMAT} LIBRARY_OUTPUT_DIRECTORY)
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME}${STATIC_PANNER_IO}_${FORMAT} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${ARTEFACTS_DIR} ${COPY_FOLDER})
endforeach()