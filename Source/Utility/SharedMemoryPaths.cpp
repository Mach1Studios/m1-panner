#include "SharedMemoryPaths.h"
#include <cstdlib>

#ifdef __APPLE__
    #include <TargetConditionals.h>
    // Foundation framework will be properly linked during build
#endif

namespace Mach1 {

#ifdef __APPLE__
// Forward declaration for Objective-C++ implementation in SharedMemoryPaths.mm
std::string getAppGroupContainerForPanner(const std::string& groupIdentifier);
#endif

std::string SharedMemoryPaths::getMemoryFileDirectory() {
    auto directories = getAllPossibleDirectories();

    // Try each directory until we find one that works or can be created
    for (const auto& dir : directories) {
        // Check if directory exists or can be created
        if (ensureDirectoryExists(dir)) {
            return dir;
        }
    }

    return "";
}

std::vector<std::string> SharedMemoryPaths::getAllPossibleDirectories() {
    std::vector<std::string> directories;

    // Priority 1: App Group container (macOS sandboxed)
    std::string appGroupPath = getAppGroupContainer();
    if (!appGroupPath.empty()) {
        directories.push_back(appGroupPath + "/Library/Caches/M1-Panner");
    }

    // Priority 2+: Platform-specific fallbacks
    auto fallbacks = getFallbackDirectories();
    directories.insert(directories.end(), fallbacks.begin(), fallbacks.end());

    return directories;
}

bool SharedMemoryPaths::ensureMemoryDirectoryExists() {
    std::string dir = getMemoryFileDirectory();
    return ensureDirectoryExists(dir);
}

bool SharedMemoryPaths::ensureDirectoryExists(const std::string& dirPath) {
    if (dirPath.empty()) {
        return false;
    }

    // Simple directory creation using system calls
    std::string command = "mkdir -p \"" + dirPath + "\"";
    int result = std::system(command.c_str());

    return result == 0;
}

std::string SharedMemoryPaths::getAppGroupContainer() {
#ifdef __APPLE__
    return getAppGroupContainerForPanner("group.com.mach1.spatial.shared");

    // Placeholder implementation:
    // @autoreleasepool {
    //     NSString* groupId = @"group.com.mach1.spatial.shared";
    //     NSURL* containerURL = [[NSFileManager defaultManager]
    //         containerURLForSecurityApplicationGroupIdentifier:groupId];
    //
    //     if (containerURL && containerURL.path) {
    //         return std::string([containerURL.path UTF8String]);
    //     }
    // }

    return "";
#else
    // App Groups only exist on Apple platforms
    return "";
#endif
}

std::vector<std::string> SharedMemoryPaths::getFallbackDirectories() {
    std::vector<std::string> directories;

#ifdef __APPLE__
    // macOS fallback paths
    const char* homeDir = std::getenv("HOME");
    if (homeDir) {
        directories.push_back(std::string(homeDir) + "/Library/Caches/M1-Panner");
    }
    directories.push_back("/tmp/M1-Panner");

#elif defined(_WIN32)
    // Windows fallback paths
    const char* appData = std::getenv("LOCALAPPDATA");
    if (appData) {
        directories.push_back(std::string(appData) + "\\M1-Panner");
    }
    const char* temp = std::getenv("TEMP");
    if (temp) {
        directories.push_back(std::string(temp) + "\\M1-Panner");
    }

#else
    // Linux/other Unix fallback paths
    const char* homeDir = std::getenv("HOME");
    if (homeDir) {
        directories.push_back(std::string(homeDir) + "/.cache/M1-Panner");
        directories.push_back(std::string(homeDir) + "/.local/share/M1-Panner");
    }
    directories.push_back("/tmp/M1-Panner");
#endif

    return directories;
}

} // namespace Mach1
