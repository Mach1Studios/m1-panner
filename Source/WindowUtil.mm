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

bool need_to_request_permissions = false;
bool permission_requested = false;

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
            // Grab the window name
            NSString *applicationName = [entry objectForKey:(id)kCGWindowOwnerName];
            NSString *windowName = [entry objectForKey:(id)kCGWindowName];

            if(applicationName != NULL) {
                NSLog(@"Found window - App: %@ | Window: %@", applicationName, windowName);

                // Convert NSString to std::string for comparison
                std::string windowNameStr = std::string([windowName UTF8String]);

                // Check if the window name matches any in our list
                for (const auto& videoPlayerName : WindowUtil::videoPlayerNames) {
                    if (windowNameStr.find(videoPlayerName) != std::string::npos) {
                        // Grab the Window Bounds
                        CGRect bounds;
                        CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)[entry objectForKey:(id)kCGWindowBounds], &bounds);

                        WindowUtil::x = bounds.origin.x;
                        WindowUtil::y = bounds.origin.y + 15;
                        WindowUtil::width = bounds.size.width;
                        WindowUtil::height = bounds.size.height - 15;
                        WindowUtil::isFound = true;

                        // Debug: Print when we find a match
                        NSLog(@"Found matching window: %@", windowName);

                        break;  // Exit the loop once we find a match
                    }
                }
            }
        }
    }
}

void WindowUtil::update() {
    // Check for screen recording permission
    if (@available(macOS 10.15, *)) {
        if (!permission_requested && !CGPreflightScreenCaptureAccess()) {
            need_to_request_permissions = true;
            NSLog(@"No screen capture access!");
        }

        if (need_to_request_permissions) {
            NSLog(@"Requesting screen capture permission...");
            CGRequestScreenCaptureAccess();
            need_to_request_permissions = false;
            permission_requested = true;
            return;
        }

        NSLog(@"Screen capture access granted");
    }

    // Set options to get all windows, including those from other apps
    CGWindowListOption listOptions = kCGWindowListOptionOnScreenOnly |
                                   kCGWindowListExcludeDesktopElements;

    CFArrayRef windowList = CGWindowListCopyWindowInfo(listOptions, kCGNullWindowID);

    if (windowList == NULL) {
        NSLog(@"Failed to get window list");
        return;
    }

    NSLog(@"Found %ld windows", CFArrayGetCount(windowList));

    if(!WindowUtil::isBusy) {
        WindowUtil::isBusy = true;
        WindowUtil::isFound = false;

        CFArrayApplyFunction(windowList, CFRangeMake(0, CFArrayGetCount(windowList)), &WindowListApplierFunction, nullptr);
        CFRelease(windowList);

        WindowUtil::isBusy = false;
    } else {
        CFRelease(windowList);
        NSLog(@"WindowUtil is busy, skipping update");
    }
}
