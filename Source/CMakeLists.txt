target_sources(${PLUGIN_NAME} PRIVATE Config.h

        ${juce_header} # TODO: Makes no sense why with the upgrade of JUCE, we got the header missing, but it builds

        TypesForDataExchange.h
        PluginEditor.cpp
        PluginEditor.h
        PluginProcessor.cpp
        PluginProcessor.h
        LegacyParameters.h
        Overlay.h
        Overlay.cpp
        PannerOSC.h
        PannerOSC.cpp
        RingBuffer.h
        WindowUtil.h
        WindowUtil.cpp
        UI/M1Checkbox.h
        UI/M1DropdownButton.h
        UI/M1DropdownMenu.h
        UI/M1Knob.h
        UI/M1PitchWheel.h
        UI/M1VolumeDisplayLine.h
        UI/OverlayReticleField.h
        UI/OverlayUIBaseComponent.h
        UI/OverlayUIBaseComponent.cpp
        UI/PannerReticleField.h
        UI/PannerUIBaseComponent.h
        UI/PannerUIBaseComponent.cpp

        # TODO: Extract sources in separate variable

        PlugSync/AudioDataSender.h
        PlugSync/AudioDataSender.cpp

        PlugSync/EnvironmentVariable.h
        PlugSync/PluginInstanceSynchronizer.cpp
        PlugSync/PluginInstanceSynchronizer.h

        PlugSync/MulticastReceiver.h
        PlugSync/MulticastReceiver.cpp

        PlugSync/ExternalMixerHandler.h
        PlugSync/ExternalMixerHandler.cpp

        PlugSync/NetworkCommunicator.h
        PlugSync/NetworkCommunicator.cpp

        PlugSync/NetworkCommunicator.h
        PlugSync/NetworkCommunicator.cpp

        # TODO: Add this later as a submodule, when we have finalized things
        )

add_subdirectory(PlugSync/shared/message_protocol_external_mixer)


get_target_property(juce_library_code ${PLUGIN_NAME} JUCE_GENERATED_SOURCES_DIRECTORY)

find_package(Boost 1.81.0)
if (Boost_FOUND)
        target_include_directories(${PLUGIN_NAME} PUBLIC AFTER
                ${Boost_INCLUDE_DIRS}
                "${juce_library_code}"
                PlugSync/shared/message_protocol_external_mixer/include)
        target_link_directories(${PLUGIN_NAME} PUBLIC ${Boost_LIBRARY_DIRS})
else()
        target_include_directories(${PLUGIN_NAME} PUBLIC AFTER
                "$ENV{BG_M1_PATH}/boost_1_81_0"
                "${juce_library_code}"
                PlugSync/shared/message_protocol_external_mixer/include)

        target_link_directories(${PLUGIN_NAME} PUBLIC "$ENV{BG_M1_PATH}/boost_1_81_0/stage/lib")
endif()

if (APPLE)
    target_sources(${PLUGIN_NAME} PRIVATE WindowUtil.mm)
endif (APPLE)