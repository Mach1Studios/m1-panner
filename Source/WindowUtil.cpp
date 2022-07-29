//
//  WindowUtil.m
//  M1-Panner
//
#ifdef  WIN32

#include "WindowUtil.h"

float WindowUtil::x = 0;
float WindowUtil::y = 0;
float WindowUtil::width = 0;
float WindowUtil::height = 0;

bool WindowUtil::isFound = false;
bool WindowUtil::isBusy = false;

#if (JucePlugin_Build_AAX || JucePlugin_Build_RTAS)
std::string WindowUtil::name = "Avid Video Engine";
#endif

#if (JucePlugin_Build_VST || JucePlugin_Build_VST3 || JucePlugin_Build_AU || JucePlugin_Build_AUv3 || JUCEPlugin_Build_Unity || JucePlugin_Build_Standalone)
//TODO: figure out better solution: reaper & reaper's video window have same name
std::string WindowUtil::name = "";
#endif

void WindowListApplierFunction(const void *inputDictionary, void *context)
{
     
}

void WindowUtil::update()
{
     
}

#endif //  WIN32
