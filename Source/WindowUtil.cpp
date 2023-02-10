//
//  WindowUtil.cpp
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
#else
//TODO: figure out better solution: reaper & reaper's video window have same name || FL Studio video window
std::string WindowUtil::name = "";
#endif

void WindowListApplierFunction(const void *inputDictionary, void *context)
{
}

void WindowUtil::update()
{
}

#endif //  WIN32
