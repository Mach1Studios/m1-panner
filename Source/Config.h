#pragma once

#define XSTR(x) STR(x)
#define STR(x) #x

/// Logic flow for determining if M1-Panner will be a dynamic I/O plugin that can change its input/output configurations
/// or if a static defined multichannel input/output configuration should be built

#pragma message "Value of INPUTS: " XSTR(JucePlugin_MaxNumInputChannels)
#pragma message "Value of OUTPUTS: " XSTR(JucePlugin_MaxNumOutputChannels)


#ifndef JucePlugin_PreferredChannelConfigurations
    /// Dynamic I/O plugin mode

    // TODO: Test for cmake i/o descriptions but add to AppConfig.h for naming of plugin

    // undefine things for next check
    #ifdef INPUT_CHANNELS
        #undef INPUT_CHANNELS
    #endif
    #ifdef OUTPUT_CHANNELS
        #undef OUTPUT_CHANNELS
    #endif
    #ifdef JucePlugin_PreferredChannelConfigurations
        #undef JucePlugin_PreferredChannelConfigurations
    #endif

    // DYNAMIC_IO_PLUGIN_MODE active
    #undef CUSTOM_CHANNEL_LAYOUT
    #define DYNAMIC_IO_PLUGIN_MODE

    // streaming plugin (stereo max) vs multichannel processing plugin
    // Please add in jucer definitions or in cmake via `ADD_DEFINITIONS(-DSTREAMING_PANNER_PLUGIN)`
    //#define STREAMING_PANNER_PLUGIN 1

#else
    /// Single instance I/O plugin mode

    // Check for jucer defined input/output config
    #if (JucePlugin_MaxNumInputChannels > 0) || (JucePlugin_MaxNumOutputChannels > 0)
        // Setup inputs and outputs for Channel Configuration
        #if JucePlugin_MaxNumInputChannels > 0
            #define INPUT_CHANNELS JucePlugin_MaxNumInputChannels
        #else
            #pragma message "ERROR: Undefined Input Configuration"
        #endif

        #if JucePlugin_MaxNumOutputChannels > 0
            #define OUTPUT_CHANNELS JucePlugin_MaxNumOutputChannels
            #define MAX_NUM_CHANNELS JucePlugin_MaxNumOutputChannels
        #else
            #pragma message "ERROR: Undefined Output Configuration"
        #endif
    #endif

    // if AAX or RTAS is setup then assume entire jucer is for DYNAMIC_IO_PLUGIN_MODE
    #if (JucePlugin_Build_AAX == 1)
        #ifdef INPUT_CHANNELS
            #undef INPUT_CHANNELS
        #endif
        #ifdef OUTPUT_CHANNELS
            #undef OUTPUT_CHANNELS
        #endif
        #ifdef JucePlugin_PreferredChannelConfigurations
            #undef JucePlugin_PreferredChannelConfigurations
        #endif
        #define DYNAMIC_IO_PLUGIN_MODE
        #pragma message("Build AAX -> Disable Custom layout chanel")
    #endif
    #if (JucePlugin_Build_RTAS == 1)
        #ifdef INPUT_CHANNELS
            #undef INPUT_CHANNELS
        #endif
        #ifdef OUTPUT_CHANNELS
            #undef OUTPUT_CHANNELS
        #endif
        #ifdef JucePlugin_PreferredChannelConfigurations
            #undef JucePlugin_PreferredChannelConfigurations
        #endif
        #define DYNAMIC_IO_PLUGIN_MODE
        #pragma message("Build RTAS -> Disable Custom layout chanel")
    #endif

#endif

// Check if Custom Config or Dynamic IO
#pragma message "Value of INPUT_CHANNELS: " XSTR(INPUT_CHANNELS)
#pragma message "Value of OUTPUT_CHANNELS: " XSTR(OUTPUT_CHANNELS)

#if (INPUT_CHANNELS > 0) && (OUTPUT_CHANNELS > 0)
    // Custom layout detected so we should unset DYNAMIC_IO_PLUGIN_MODE
    #undef DYNAMIC_IO_PLUGIN_MODE
    #undef STREAMING_PANNER_PLUGIN
    #define CUSTOM_CHANNEL_LAYOUT
#else
    // DYNAMIC_IO_PLUGIN_MODE active
    #undef CUSTOM_CHANNEL_LAYOUT
    #define DYNAMIC_IO_PLUGIN_MODE
#endif

#ifndef PI
#define PI       3.14159265358979323846
#endif
