//
//  WindowUtil.h
//  M1-Panner
//
#pragma once
#include <string>
#include <vector>
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

    // List of video player window titles to match against.
    static std::vector<std::string> videoPlayerNames;

    // Some DAWs expose their video window identity through the owning process
    // while leaving the window title empty.
    static std::vector<std::string> videoPlayerOwnerNames;

    static void update();
};
