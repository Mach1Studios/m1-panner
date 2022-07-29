#ifndef TYPESFORDATAEXCHANGE_H
#define TYPESFORDATAEXCHANGE_H

#include "Mach1Encode.h"

struct PannerSettings {

    
    float x = 0;
	float y = 100;
	float z = 0; // elevation as well
	float azimuth = 0;
	float diverge = 70.7;
	float gain = 6;
	float stereoRotate = 0;
	float stereoSpread = 50;
	float stereoPan = 0;
    Mach1EncodePannerMode pannerMode = 0;
    Mach1EncodeInputModeType inputType = Mach1EncodeInputModeStereo;
    Mach1EncodeOutputModeType outputType = Mach1EncodeOutputModeM1Spatial;
    
	bool overlay = false;
	 
	Mach1Encode* m1Encode = nullptr;
};

struct MixerSettings {
	float yaw = 0;
	float pitch = 0;
	float roll = 0;

	float generatedCoeffs[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int outputChannelCount = 0;
};

#endif
