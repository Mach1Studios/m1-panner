#pragma once

#define XSTR(x) STR(x)
#define STR(x) #x

/// Logic flow for determining if M1-Panner will be a dynamic I/O plugin that can change its input/output configurations
/// or if a static defined multichannel input/output configuration should be built

#pragma message "Value of INPUTS: " XSTR(JucePlugin_MaxNumInputChannels)
#pragma message "Value of OUTPUTS: " XSTR(JucePlugin_MaxNumOutputChannels)


#ifndef JucePlugin_PreferredChannelConfigurations
    /// Dynamic I/O plugin mode

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

    // streaming plugin (stereo max) vs multichannel processing plugin
    // Please add in jucer definitions or in cmake via `ADD_DEFINITIONS(-DSTREAMING_PANNER_PLUGIN)`
    #ifdef STREAMING_PANNER_PLUGIN
        /// STREAMING_PANNER_PLUGIN mode active
    #else
        /// DYNAMIC_IO_PLUGIN_MODE active
        #undef CUSTOM_CHANNEL_LAYOUT
        #define DYNAMIC_IO_PLUGIN_MODE
    #endif

#else
    /// Single instance I/O plugin mode
    // TODO: Test for cmake i/o descriptions but add to AppConfig.h for naming of plugin

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
    #undef CUSTOM_CHANNEL_LAYOUT
    
    #ifdef STREAMING_PANNER_PLUGIN
        /// STREAMING_PANNER_PLUGIN mode active
    #else
        /// DYNAMIC_IO_PLUGIN_MODE active
        #define DYNAMIC_IO_PLUGIN_MODE
    #endif

#endif

#ifndef PI
#define PI       3.14159265358979323846
#endif

/// Static Color Scheme
#define M1_ACTION_YELLOW 255, 198, 30
// 204, 204, 204 seen on ENABLED knobs in legacy as well
#define ENABLED_PARAM 190, 190, 190
#define DISABLED_PARAM 63, 63, 63
#define BACKGROUND_GREY 40, 40, 40

#define GRID_LINES_1_RGBA 68, 68, 68, 51//0.2 opacity //small grid lines
#define GRID_LINES_2 68, 68, 68
#define GRID_LINES_3_RGBA 102, 102, 102, 178//0.7 opacity
#define GRID_LINES_4_RGB 133, 133, 133

#define LABEL_TEXT_COLOR 163, 163, 163
#define REF_LABEL_TEXT_COLOR 93, 93, 93
#define HIGHLIGHT_COLOR LABEL_TEXT_COLOR
#define HIGHLIGHT_TEXT_COLOR 0, 0, 0
#define APP_LABEL_TEXT_COLOR GRID_LINES_4_RGB

#define METER_RED 178, 24, 23
#define METER_YELLOW 220, 174, 37
#define METER_GREEN 67, 174, 56
