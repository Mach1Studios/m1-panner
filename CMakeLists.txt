cmake_minimum_required(VERSION 3.13.0)
project(M1-Panner VERSION 2.0.0)
add_subdirectory(JUCE)

IF(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/JUCE_deps)
    set(AAX_PATH "JUCE_deps/AAX_SDK_2p3p2")
    set(VST2_PATH "JUCE_deps/VST_SDK_vst2/VST2_SDK")
    message("-- JUCE Dependencies Found: ${PROJECT_SOURCE_DIR}/JUCE_deps")
ELSE()
    message("Warning: Did not find additional JUCE deps submodule. Please add the appropriate SDKs manually.")
ENDIF()

OPTION(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT "Compiles a single I/O static setup pugin, requires setting CONFIG_IO for example: `-DINPUTS=1 -DOUTPUTS=8`; n,N where n=number_of_input_channels and N=number_of_output_channels" OFF)
IF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT)
    MESSAGE("Make sure the 'INPUTS' and 'OUTPUTS' are set!")
ENDIF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT)

OPTION(BUILD_STREAMING_PANNER_PLUGIN "Compiles the plugin UI and processing code for external spatial audio processing" OFF) # Disabled by default
OPTION(BUILD_DYNAMIC_IO_PLUGIN_MODE "Compiles the plugin UI and processing code for re-declarable I/O" OFF) # Enabled by default
OPTION(BUILD_ITD_PARAMETERS "Adds the experimental ITD processing parameters and UI" OFF) # Disabled by default
OPTION(BUILD_VST3 "Compile VST3 plugin type" OFF) # Enabled by default

# These are used to re-apply brackets for the JucePlugin_PreferredChannelConfig
SET(LBRACKET_LITERAL "{")
SET(RBRACKET_LITERAL "}")

IF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT AND (INPUTS GREATER 0 AND OUTPUTS GREATER 0))
    SET(PANNER_IO "(${INPUTS}-${OUTPUTS})")
    SET(PANNER_NAME "${CMAKE_PROJECT_NAME}${PANNER_IO}")
    SET(CUSTOM_BUNDLE_ID "com.mach1.panner.i${INPUTS}o${OUTPUTS}")
    SET(CUSTOM_PLUGIN_CODE "MP${INPUTS}${OUTPUTS}")
    ADD_DEFINITIONS(JucePlugin_PreferredChannelConfig "${LBRACKET_LITERAL}${INPUTS},${OUTPUTS}${RBRACKET_LITERAL}")
    # TODO: Change output name to reflect
ELSEIF(BUILD_WITH_CUSTOM_CHANNEL_LAYOUT AND NOT (INPUTS GREATER 0 AND OUTPUTS GREATER 0))
    MESSAGE(SEND_ERROR "INPUTS and OUTPUTS are not defined!")
ELSE()
    SET(PANNER_NAME "${CMAKE_PROJECT_NAME}")
    SET(CUSTOM_BUNDLE_ID "com.mach1.panner")
    SET(CUSTOM_PLUGIN_CODE "M1PN")
    IF(BUILD_STREAMING_PANNER_PLUGIN)
        ADD_DEFINITIONS(-DSTREAMING_PANNER_PLUGIN)
    ENDIF(BUILD_STREAMING_PANNER_PLUGIN)

    IF(BUILD_DYNAMIC_IO_PLUGIN_MODE)
        ADD_DEFINITIONS(-DDYNAMIC_IO_PLUGIN_MODE)
    ENDIF(BUILD_DYNAMIC_IO_PLUGIN_MODE)
ENDIF()

IF(BUILD_ITD_PARAMETERS)
    ADD_DEFINITIONS(-DITD_PARAMETERS)
ENDIF(BUILD_ITD_PARAMETERS)

# check which formats we want to build
IF(BUILD_VST3)
    list(APPEND FORMATS "VST3")
ENDIF(BUILD_VST3)

IF(BUILD_VST2 AND VST2_PATH)
    list(APPEND FORMATS "VST2")
    juce_set_vst2_sdk_path(${VST2_PATH})
ENDIF()

IF(BUILD_AAX AND AAX_PATH)
    list(APPEND FORMATS "AAX")
    juce_set_aax_sdk_path(${AAX_PATH})
ENDIF()

IF(BUILD_AU AND APPLE)
    list(APPEND FORMATS "AU")
ENDIF()

# add the plugin targets
juce_add_plugin(${PANNER_NAME}
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
                BUNDLE_ID ${CUSTOM_BUNDLE_ID}
                PLUGIN_NAME ${PANNER_NAME}
                PRODUCT_NAME ${PANNER_NAME})

# add the sources
add_subdirectory(Resources)
add_subdirectory(Source)

# add required flags
target_link_libraries(${PANNER_NAME} PRIVATE juce::juce_recommended_warning_flags juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
target_link_libraries(${PANNER_NAME} PRIVATE juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats juce::juce_audio_plugin_client juce::juce_audio_processors juce::juce_audio_utils juce::juce_core juce::juce_cryptography juce::juce_data_structures juce::juce_dsp juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra juce::juce_opengl juce::juce_osc juce::juce_product_unlocking juce_murka m1_orientation_client)
target_compile_definitions(${PANNER_NAME}
    PUBLIC 
    JUCE_WEB_BROWSER=0)
juce_generate_juce_header(${PANNER_NAME})

# definitions to replace the `JucePluginDefines.h`
SET(JucePlugin_Name ${PANNER_NAME})
SET(JucePlugin_Desc ${PANNER_NAME})

# setup the copying to the output folder
IF(APPLE)
    set(COPY_FOLDER ${CMAKE_SOURCE_DIR}/Builds/MacOSX)
ELSEIF(WIN32)
    set(COPY_FOLDER ${CMAKE_SOURCE_DIR}/Builds/VisualStudio2015)
ENDIF()

FOREACH(FORMAT ${FORMATS})
    get_target_property(ARTEFACTS_DIR ${PANNER_NAME}_${FORMAT} LIBRARY_OUTPUT_DIRECTORY)
    add_custom_command(TARGET ${PANNER_NAME}_${FORMAT} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${ARTEFACTS_DIR} ${COPY_FOLDER})
ENDFOREACH()