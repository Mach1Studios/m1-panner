#pragma once

#include <string>
#include <vector>

namespace Mach1 {

/**
 * Shared memory path utilities for M1-Panner
 * Ensures .mem files are written to locations that m1-system-helper can read
 */
class SharedMemoryPaths {
public:
    /**
     * Get the directory where M1-Panner should write .mem files
     * Returns App Group container on macOS (sandboxed), fallback paths otherwise
     */
    static std::string getMemoryFileDirectory();

    /**
     * Get all possible directories to try (in priority order)
     */
    static std::vector<std::string> getAllPossibleDirectories();

    /**
     * Create the memory file directory if it doesn't exist
     */
    static bool ensureMemoryDirectoryExists();

    /**
     * Ensure a specific directory exists (helper function)
     */
    static bool ensureDirectoryExists(const std::string& dirPath);

private:
    static std::string getAppGroupContainer();
    static std::vector<std::string> getFallbackDirectories();
};

} // namespace Mach1
