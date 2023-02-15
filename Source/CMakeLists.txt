target_sources(${PLUGIN_NAME} PRIVATE    Config.h
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
                                    UI/PannerUIBaseComponent.cpp)

if(APPLE)
    target_sources(${PLUGIN_NAME} PRIVATE WindowUtil.mm)
endif(APPLE)