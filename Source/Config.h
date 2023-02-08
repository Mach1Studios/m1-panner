#pragma once

#define XSTR(x) STR(x)
#define STR(x) #x

/// Single instance I/O plugin mode

// Check for jucer defined input/output config
#if (JucePlugin_MaxNumInputChannels > 0) || (JucePlugin_MaxNumOutputChannels > 0)
    // Setup inputs and outputs for Channel Configuration
    #if JucePlugin_MaxNumInputChannels > 0
        #define INPUT_CHANNELS JucePlugin_MaxNumInputChannels
    #else
        #error ERROR: Undefined Input Configuration from JUCER
    #endif

    #if JucePlugin_MaxNumOutputChannels > 0
        #define OUTPUT_CHANNELS JucePlugin_MaxNumOutputChannels
        #define MAX_NUM_CHANNELS JucePlugin_MaxNumOutputChannels
    #else
        #error ERROR: Undefined Output Configuration from JUCER
    #endif

    // if AAX or RTAS is setup alongside custom channel single mode than error
    #if (JucePlugin_Build_AAX == 1)
        #error ERROR: Build AAX -> Disable Custom layout chanel
    #endif
    #if (JucePlugin_Build_RTAS == 1)
        #error ERROR: Build RTAS -> Disable Custom layout chanel
    #endif
#else
    // We are likely using CMake to define a layout which does not update the `JucePlugin_MaxNumInputChannels` or `JucePlugin_MaxNumOutputChannels` definitions
    #if (INPUT_CHANNELS > 0) || (OUTPUT_CHANNELS > 0)
        // Setup inputs and outputs for Channel Configuration
        #if INPUT_CHANNELS > 0
            #define JucePlugin_MaxNumInputChannels INPUT_CHANNELS
        #else
            #error ERROR: Undefined Input Configuration from CMAKE
        #endif

        #if OUTPUT_CHANNELS > 0
            #define JucePlugin_MaxNumOutputChannels OUTPUT_CHANNELS
            #define MAX_NUM_CHANNELS OUTPUT_CHANNELS
        #else
            #error ERROR: Undefined Output Configuration from CMAKE
        #endif
        
        // if AAX or RTAS is setup alongside custom channel single mode than error
        #if (JucePlugin_Build_AAX == 1)
            #error ERROR: Build AAX -> Disable Custom layout chanel
        #endif
        #if (JucePlugin_Build_RTAS == 1)
            #error ERROR: Build RTAS -> Disable Custom layout chanel
        #endif
    #endif
#endif

// Check if Custom Config
#pragma message "Value of INPUTS: " XSTR(JucePlugin_MaxNumInputChannels)
#pragma message "Value of OUTPUTS: " XSTR(JucePlugin_MaxNumOutputChannels)
#pragma message "Value of INPUT_CHANNELS: " XSTR(INPUT_CHANNELS)
#pragma message "Value of OUTPUT_CHANNELS: " XSTR(OUTPUT_CHANNELS)

#if (INPUT_CHANNELS > 0) && (OUTPUT_CHANNELS > 0)
    #define CUSTOM_CHANNEL_LAYOUT
#else
    #undef CUSTOM_CHANNEL_LAYOUT
#endif

#ifdef CUSTOM_CHANNEL_LAYOUT
    #pragma message "CUSTOM_CHANNEL_LAYOUT Active"
#endif

#ifdef ITD_PARAMETERS
    #pragma message "ITD_PARAMETERS Active"
#endif

// ---

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
#define OVERLAY_YAW_REF_RGBA 93, 93, 93, 51

#define LABEL_TEXT_COLOR 163, 163, 163
#define REF_LABEL_TEXT_COLOR 93, 93, 93
#define HIGHLIGHT_COLOR LABEL_TEXT_COLOR
#define HIGHLIGHT_TEXT_COLOR 0, 0, 0
#define APP_LABEL_TEXT_COLOR GRID_LINES_4_RGB

#define METER_RED 178, 24, 23
#define METER_YELLOW 220, 174, 37
#define METER_GREEN 67, 174, 56
