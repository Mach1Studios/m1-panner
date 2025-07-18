#include "SharedMemoryPaths.h"
#include <string>

#ifdef __APPLE__
#import <Foundation/Foundation.h>

namespace Mach1 {

// Objective-C++ implementation for App Group container access
std::string getAppGroupContainerForPanner(const std::string& groupIdentifier) {
    @autoreleasepool {
        NSString* groupId = [NSString stringWithUTF8String:groupIdentifier.c_str()];
        NSURL* containerURL = [[NSFileManager defaultManager]
            containerURLForSecurityApplicationGroupIdentifier:groupId];

        if (containerURL && containerURL.path) {
            std::string result = std::string([containerURL.path UTF8String]);
            NSLog(@"[SharedMemoryPaths] App Group container found: %@", containerURL.path);
            return result;
        }

        NSLog(@"[SharedMemoryPaths] App Group container not found for: %@", groupId);
        return "";
    }
}

} // namespace Mach1

#else

namespace Mach1 {

// Non-Apple platforms
std::string getAppGroupContainerForPanner(const std::string& groupIdentifier) {
    return "";
}

} // namespace Mach1

#endif
