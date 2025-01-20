//
//  WindowUtil.m
//  M1-Panner
//

#import <Foundation/Foundation.h>
#import <AppKit/NSAlert.h>
#import "WindowUtil.h"

float WindowUtil::x = 0;
float WindowUtil::y = 0;
float WindowUtil::width = 0;
float WindowUtil::height = 0;

bool WindowUtil::isFound = false;
bool WindowUtil::isBusy = false;

// List of common video player window names
std::vector<std::string> WindowUtil::videoPlayerNames = {
    "Avid Video Engine",
    "Video Engine",
    "Video",
    "Video Player",
    "FL Studio Video Player",
    "Logic Pro Video",
    "Studio One Video Player",
    "Cubase Video Player"
};

void WindowListApplierFunction(const void *inputDictionary, void *context) {
    NSDictionary *entry = (__bridge NSDictionary*)inputDictionary;

    // The flags that we pass to CGWindowListCopyWindowInfo will automatically filter out most undesirable windows.
    // However, it is possible that we will get back a window that we cannot read from, so we'll filter those out manually.
    int sharingState = [[entry objectForKey:(id)kCGWindowSharingState] intValue];
    if(sharingState != kCGWindowSharingNone) {
        NSString *sVisible = [entry objectForKey:(id)kCGWindowIsOnscreen];
        float isVisible = [sVisible floatValue];

        if(isVisible==1) {
            // Grab the application name, but since it's optional we need to check before we can use it.
            NSString *applicationName = [entry objectForKey:(id)kCGWindowOwnerName];
            if(applicationName != NULL) {
                // Convert NSString to std::string for comparison
                std::string appNameStr = std::string([applicationName UTF8String]);

                // Check if the window name matches any in our list
                for (const auto& videoPlayerName : WindowUtil::videoPlayerNames) {
                    if (appNameStr.find(videoPlayerName) != std::string::npos) {
                        // Grab the Window Bounds
                        CGRect bounds;
                        CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)[entry objectForKey:(id)kCGWindowBounds], &bounds);

                        WindowUtil::x = bounds.origin.x;
                        WindowUtil::y = bounds.origin.y + 15;
                        WindowUtil::width = bounds.size.width;
                        WindowUtil::height = bounds.size.height - 15;
                        WindowUtil::isFound = true;
                        break;  // Exit the loop once we find a match
                    }
                }
            }
        }
    }
}

void WindowUtil::update() {
    CGWindowListOption listOptions;
    CFArrayRef windowList = CGWindowListCopyWindowInfo(listOptions, kCGNullWindowID);

    if(!WindowUtil::isBusy) {
        WindowUtil::isBusy = true;
        WindowUtil::isFound = false;

        CFArrayApplyFunction(windowList, CFRangeMake(0, CFArrayGetCount(windowList)), &WindowListApplierFunction, nullptr);
        CFRelease(windowList);

        WindowUtil::isBusy = false;
    }
}
