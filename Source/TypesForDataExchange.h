#ifndef TYPESFORDATAEXCHANGE_H
#define TYPESFORDATAEXCHANGE_H

#include "Mach1Encode.h"

struct PannerSettings {
    Mach1EncodeInputModeType inputType = Mach1EncodeInputModeStereo;
    Mach1EncodeOutputModeType outputType = Mach1EncodeOutputModeM1Spatial_8;
    Mach1EncodePannerMode pannerMode = Mach1EncodePannerModeIsotropicLinear;
    float x = 0;
	float y = 100;
	float azimuth = 0;
    float elevation = 0; // also known as `z`
    float diverge = 70.7;
	float gain = 6;
	float stereoOrbitAzimuth = 0;
	float stereoSpread = 50;
	float stereoInputBalance = 0;
    bool autoOrbit = true;
	bool overlay = false;
    bool isotropicMode = true;
    bool equalpowerMode = false;
	 
	Mach1Encode* m1Encode = nullptr;
};

struct MixerSettings {
	int monitor_input_channel_count = 8;
	int monitor_output_channel_count = 2;
	float yaw = 0;
	float pitch = 0;
	float roll = 0;
    int monitor_mode = 0;
};

#endif
