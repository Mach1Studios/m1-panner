//
//  WindowUtil.m
//  M1-Panner
//

#import "WindowUtil.h"
#import <Foundation/Foundation.h>

#import <AppKit/NSAlert.h>


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
//TODO: figure out better solution: reaper & reaper's video window have same name || FL Studio video window
std::string WindowUtil::name = "";
#endif

void WindowListApplierFunction(const void *inputDictionary, void *context)
{
    NSDictionary *entry = (__bridge NSDictionary*)inputDictionary;
    NSString *appName = @(WindowUtil::name.c_str());
    
    // The flags that we pass to CGWindowListCopyWindowInfo will automatically filter out most undesirable windows.
    // However, it is possible that we will get back a window that we cannot read from, so we'll filter those out manually.
    int sharingState = [[entry objectForKey:(id)kCGWindowSharingState] intValue];
    if(sharingState != kCGWindowSharingNone)
    {
    NSString *sVisible = [entry objectForKey:(id)kCGWindowIsOnscreen];
    float isVisible = [sVisible floatValue];
    
        if(isVisible==1)
        {
            // Grab the application name, but since it's optional we need to check before we can use it.
            NSString *applicationName = [entry objectForKey:(id)kCGWindowOwnerName];
            if(applicationName != NULL)
            {
                if ([applicationName isEqualToString:appName]){
                    
                    // Grab the Window Bounds, it's a dictionary in the array, but we want to display it as a string
                    CGRect bounds;
                    CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)[entry objectForKey:(id)kCGWindowBounds], &bounds);
                    
                    //     NSAlert *alert = [[[NSAlert alloc] init] autorelease];
                    //      [alert setMessageText:applicationName];
                    //    [alert runModal];
       
                    WindowUtil::x = bounds.origin.x;
                    WindowUtil::y = bounds.origin.y + 15;
                    WindowUtil::width = bounds.size.width;
                    WindowUtil::height = bounds.size.height - 15;
                    WindowUtil::isFound = true;
                    
                    
                }
                
            }
            
        } // visible
    }
}

void WindowUtil::update()
{
    CGWindowListOption listOptions;
    CFArrayRef windowList = CGWindowListCopyWindowInfo(listOptions, kCGNullWindowID);
    
    if(!WindowUtil::isBusy)
    {
        WindowUtil::isBusy = true;
        WindowUtil::isFound = false;

        CFArrayApplyFunction(windowList, CFRangeMake(0, CFArrayGetCount(windowList)), &WindowListApplierFunction, nullptr);
        CFRelease(windowList);
        
        WindowUtil::isBusy = false;
    }
}
