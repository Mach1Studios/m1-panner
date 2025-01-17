//
//  WindowUtil.h
//  M1-Panner
//
#pragma once
#include "PluginProcessor.h"
#include <string>
#include <vector>

class WindowUtil
{
public:
    static bool isBusy;
    static bool isFound;

    static float x;
    static float y;
    static float width;
    static float height;

    // List of video player window names to match against
    static std::vector<std::string> videoPlayerNames;

    static void update();
};
