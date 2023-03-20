target_sources(${PLUGIN_NAME} PRIVATE Config.h
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

target_include_directories(${PLUGIN_NAME} PUBLIC
        "$ENV{BG_M1_PATH}/boost_1_81_0"
        PlugSync/shared/message_protocol_external_mixer/include)

target_link_directories(${PLUGIN_NAME} PUBLIC "$ENV{BG_M1_PATH}/boost_1_81_0/stage/lib")

if (APPLE)
    target_sources(${PLUGIN_NAME} PRIVATE WindowUtil.mm)
endif (APPLE)