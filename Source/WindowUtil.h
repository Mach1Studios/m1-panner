//
//  WindowUtil.h
//  M1-Panner
//
#include <string>
#include "PluginProcessor.h"

class WindowUtil
{    
public:
    static bool isBusy;
    
    static bool isFound;
    
    static float x;
    static float y;
    static float width;
    static float height;
    
    static std::string name;
    
    static void update();
};
