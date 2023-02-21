#ifndef TYPESFORDATAEXCHANGE_H
#define TYPESFORDATAEXCHANGE_H

#include "Mach1Encode.h"

struct PannerSettings {
    // Should we remove these to force setup through `m1Encode` object instead?
    Mach1EncodeInputModeType inputType = Mach1EncodeInputModeStereo;
    Mach1EncodeOutputModeType outputType = Mach1EncodeOutputModeM1Spatial_8;
    Mach1EncodePannerMode pannerMode = Mach1EncodePannerModePeriphonicLinear;
    // ?
    
    float x = 0.;
	float y = 70.7;
	float azimuth = 0.;
    float elevation = 0.; // also known as `z`
    float diverge = 50;
	float gain = 6.;
	float stereoOrbitAzimuth = 0.;
	float stereoSpread = 50.;
	float stereoInputBalance = 0.;
    bool autoOrbit = true;
	bool overlay = false;
    bool isotropicMode = false;
    bool equalpowerMode = false;
    
#ifdef ITD_PARAMETERS
    bool itdActive = false;
    int delayTime = 600;
    float delayDistance = 1.0;
#endif

	Mach1Encode* m1Encode = nullptr;
    
};

struct MixerSettings {
	int monitor_input_channel_count;
	int monitor_output_channel_count;
	float yaw;
	float pitch;
	float roll;
    int monitor_mode;
};

#endif
