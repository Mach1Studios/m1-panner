#pragma once

#include <JuceHeader.h>
#include "LockFreeRingBuffer.h"
#include <atomic>
#include <memory>
#include <thread>
#include <cstdlib>
#include <string>

#if JUCE_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
#endif

/**
 * StreamingMode
 * 
 * Manages the streaming mode for m1-panner where:
 * - Plugin I/O remains stereo for DAW compatibility
 * - Internally generates Mach1 multichannel encoded buffer
 * - Writes multichannel buffer to memory-mapped ring buffer
 * - Registers with m1-system-helper via OSC
 * 
 * Usage:
 * - Call prepareStreaming() from prepareToPlay()
 * - Call writeEncodedAudio() from processBlock() with the encoded multichannel buffer
 * - Call updateHeader() to sync spatial parameters
 * - Call shutdown() from destructor
 */

namespace Mach1 {

class StreamingMode {
public:
    // Streaming buffer configuration
    static constexpr uint32_t DEFAULT_RING_BUFFER_SECONDS = 2;  // 2 seconds of audio
    static constexpr uint32_t MAX_CHANNELS = 14;                // M1Spatial_14 max
    static constexpr uint32_t MIN_CHANNELS = 4;                 // M1Spatial_4 min
    
    StreamingMode() {
        DBG("[StreamingMode] Instance created");
    }
    ~StreamingMode() {
        DBG("[StreamingMode] Instance destroying");
        shutdown();
    }
    
    /**
     * Generate a new UUID for this panner instance
     * Called once when plugin is first instantiated (not from saved state)
     */
    static juce::String generatePannerUuid() {
        return juce::Uuid().toString();
    }
    
    /**
     * Get the session ID (host process ID as string)
     * This groups panners from the same DAW session
     */
    static juce::String getSessionId() {
        #if JUCE_WINDOWS
            return juce::String(static_cast<juce::int64>(GetCurrentProcessId()));
        #else
            return juce::String(static_cast<juce::int64>(getpid()));
        #endif
    }
    
    /**
     * Prepare streaming mode - creates memory-mapped file and ring buffer
     * Call from prepareToPlay()
     * 
     * @param uniqueInstanceName Unique instance name (from generateUniqueInstanceName)
     * @param numOutputChannels Number of Mach1 output channels (4, 8, or 14)
     * @param sampleRate Audio sample rate
     * @param maxBlockSize Maximum expected block size
     * @return true if streaming mode successfully initialized
     */
    bool prepareStreaming(const juce::String& uniqueInstanceName, 
                         uint32_t numOutputChannels,
                         double sampleRate, 
                         int maxBlockSize) {
        // Validate parameters
        if (numOutputChannels < MIN_CHANNELS || numOutputChannels > MAX_CHANNELS) {
            DBG("[StreamingMode] Invalid channel count: " + juce::String(numOutputChannels));
            return false;
        }
        
        this->instanceName = uniqueInstanceName;
        this->sessionId = getSessionId();
        this->numChannels = numOutputChannels;
        this->sampleRate = static_cast<uint32_t>(sampleRate);
        this->maxBlockSize = static_cast<uint32_t>(maxBlockSize);
        
        // Calculate buffer size (2 seconds of audio, rounded up to block boundary)
        const uint32_t bufferSamples = static_cast<uint32_t>(sampleRate * DEFAULT_RING_BUFFER_SECONDS);
        const uint32_t alignedBufferSamples = ((bufferSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;
        
        // Generate memory-mapped file path using instance name
        mmapPath = getStreamFilePath(instanceName);
        
        // Calculate required size
        const size_t requiredSize = LockFreeRingBuffer::getRequiredSize(numChannels, alignedBufferSamples);
        
        // Create or open the memory-mapped file
        juce::File mmapFile(mmapPath);
        
        // Ensure parent directory exists
        mmapFile.getParentDirectory().createDirectory();
        
        // Delete existing file if it exists
        if (mmapFile.exists()) {
            mmapFile.deleteFile();
        }
        
        // Create the file with required size
        if (!mmapFile.create()) {
            DBG("[StreamingMode] Failed to create mmap file: " + mmapPath);
            return false;
        }
        
        // Resize file to required size
        {
            juce::FileOutputStream fos(mmapFile);
            if (!fos.openedOk()) {
                DBG("[StreamingMode] Failed to open mmap file for sizing");
                return false;
            }
            fos.setPosition(requiredSize - 1);
            fos.writeByte(0);
        }
        
        // Memory-map the file
        mappedFile = std::make_unique<juce::MemoryMappedFile>(
            mmapFile, 
            juce::MemoryMappedFile::readWrite,
            requiredSize
        );
        
        if (!mappedFile->getData()) {
            DBG("[StreamingMode] Failed to memory-map file");
            mappedFile.reset();
            return false;
        }
        
        // Initialize ring buffer
        if (!ringBuffer.initialize(
                mappedFile->getData(),
                requiredSize,
                numChannels,
                alignedBufferSamples,
                this->sampleRate,
                this->maxBlockSize,
                true  // Initialize header (we are the producer)
            )) {
            DBG("[StreamingMode] Failed to initialize ring buffer");
            mappedFile.reset();
            return false;
        }
        
        // Set panner identification in header
        StreamHeader* hdr = ringBuffer.getHeader();
        if (hdr) {
            std::strncpy(hdr->pannerUuid, instanceName.toRawUTF8(), sizeof(hdr->pannerUuid) - 1);
            std::strncpy(hdr->sessionId, sessionId.toRawUTF8(), sizeof(hdr->sessionId) - 1);
        }
        
        // Allocate temporary buffer for channel pointers
        tempChannelPointers.resize(numChannels);
        
        streamingPrepared.store(true, std::memory_order_release);
        
        DBG("[StreamingMode] Prepared streaming: " + instanceName + 
            " channels=" + juce::String(numChannels) +
            " SR=" + juce::String(this->sampleRate) +
            " path=" + mmapPath);
        
        return true;
    }
    
    /**
     * Write encoded multichannel audio to the ring buffer
     * Call from processBlock() - must be RT-safe (no allocations, no locks)
     * 
     * @param encodedBuffer Multichannel buffer with Mach1 encoded audio
     * @param numSamples Number of samples in the buffer
     * @return true if write successful
     */
    bool writeEncodedAudio(const juce::AudioBuffer<float>& encodedBuffer, int numSamples) {
        if (!streamingPrepared.load(std::memory_order_acquire)) {
            return false;
        }
        
        // Setup channel pointers (no allocation - vector pre-sized in prepareStreaming)
        const int bufferChannels = encodedBuffer.getNumChannels();
        const int channelsToWrite = juce::jmin(bufferChannels, static_cast<int>(numChannels));
        
        for (int ch = 0; ch < channelsToWrite; ++ch) {
            tempChannelPointers[ch] = encodedBuffer.getReadPointer(ch);
        }
        
        // Zero any remaining channels if buffer has fewer channels than expected
        static float zeroBuffer[8192] = {0};  // Static zero buffer for unused channels
        for (int ch = channelsToWrite; ch < static_cast<int>(numChannels); ++ch) {
            tempChannelPointers[ch] = zeroBuffer;
        }
        
        return ringBuffer.write(tempChannelPointers.data(), static_cast<uint32_t>(numSamples));
    }
    
    /**
     * Write raw channel pointers to the ring buffer
     * Alternative to writeEncodedAudio for when you have raw pointers
     */
    bool writeEncodedAudio(const float* const* channelData, int numSamples) {
        if (!streamingPrepared.load(std::memory_order_acquire)) {
            return false;
        }
        return ringBuffer.write(channelData, static_cast<uint32_t>(numSamples));
    }
    
    /**
     * Touch the memory file to update its modification time
     * Call periodically from a Timer callback (NOT from processBlock)
     * This keeps the file "fresh" so the helper doesn't consider it stale.
     * 
     * The M1MemoryShareTracker considers files older than 120 seconds as stale.
     * Call this every 30-60 seconds to be safe.
     */
    void touchFile() {
        if (!streamingPrepared.load(std::memory_order_acquire)) {
            return;
        }
        
        juce::File file(mmapPath);
        if (file.exists()) {
            // Touch the file by setting its modification time to now
            file.setLastModificationTime(juce::Time::getCurrentTime());
        }
    }
    
    /**
     * Update the stream header with current encode coefficients and parameters
     * Call from processBlock() or timer callback
     * RT-safe (just memory writes, no allocations)
     */
    void updateHeader(const std::vector<std::vector<float>>& encodeGains,
                     int inputMode, int outputMode, int pannerMode,
                     float azimuth, float elevation, float diverge, float gain,
                     bool autoOrbit, float stereoOrbitAzimuth, float stereoSpread, float stereoBalance,
                     double playheadPosition, bool isPlaying) {
        if (!streamingPrepared.load(std::memory_order_acquire)) {
            return;
        }
        
        StreamHeader* hdr = ringBuffer.getHeader();
        if (!hdr) return;
        
        // Copy encode gains (up to 2 input channels, up to 14 output channels)
        const size_t inputChans = std::min(encodeGains.size(), size_t(2));
        for (size_t i = 0; i < inputChans; ++i) {
            const size_t outputChans = std::min(encodeGains[i].size(), size_t(14));
            for (size_t o = 0; o < outputChans; ++o) {
                hdr->encodeGains[i][o] = encodeGains[i][o];
            }
        }
        
        hdr->encodeInputMode = static_cast<uint32_t>(inputMode);
        hdr->encodeOutputMode = static_cast<uint32_t>(outputMode);
        hdr->encodePannerMode = static_cast<uint32_t>(pannerMode);
        
        hdr->azimuth = azimuth;
        hdr->elevation = elevation;
        hdr->diverge = diverge;
        hdr->gain = gain;
        
        hdr->autoOrbit = autoOrbit ? 1 : 0;
        hdr->stereoOrbitAzimuth = stereoOrbitAzimuth;
        hdr->stereoSpread = stereoSpread;
        hdr->stereoInputBalance = stereoBalance;
        
        hdr->playheadPositionSeconds = playheadPosition;
        hdr->isPlaying = isPlaying ? 1 : 0;
        hdr->lastWriteTimestamp = static_cast<uint64_t>(juce::Time::currentTimeMillis());
    }
    
    /**
     * Shutdown streaming mode - cleanup resources
     * Call from destructor or when disabling streaming
     */
    void shutdown() {
        if (streamingPrepared.load(std::memory_order_acquire)) {
            // Mark stream as inactive
            ringBuffer.deactivate();
            
            streamingPrepared.store(false, std::memory_order_release);
            
            // Release memory mapping
            mappedFile.reset();
            
            // Optionally delete the file
            juce::File mmapFile(mmapPath);
            if (mmapFile.exists()) {
                mmapFile.deleteFile();
            }
            
            DBG("[StreamingMode] Shutdown: " + instanceName);
        }
    }
    
    /**
     * Check if streaming is currently active
     */
    bool isStreamingActive() const {
        return streamingPrepared.load(std::memory_order_acquire);
    }
    
    /**
     * Get the memory-mapped file path for registration with helper
     */
    juce::String getMmapPath() const { return mmapPath; }
    
    /**
     * Get the panner instance name (used for registration and file naming)
     */
    juce::String getPannerInstanceName() const { return instanceName; }
    
    /**
     * Get the stored session ID for this instance
     */
    juce::String getStoredSessionId() const { return sessionId; }
    
    /**
     * Get current channel count
     */
    uint32_t getNumChannels() const { return numChannels; }
    
    /**
     * Get sample rate
     */
    uint32_t getSampleRate() const { return sampleRate; }
    
    /**
     * Get max block size
     */
    uint32_t getMaxBlockSize() const { return maxBlockSize; }
    
    /**
     * Get the primary shared directory for .mem files
     * Matches the paths expected by M1MemoryShareTracker in m1-system-helper
     * 
     * Priority order (matching SharedPathUtils):
     * - macOS: App Group container (for sandboxed AAX), then ~/Library/Caches/M1-Panner
     * - Windows: %LOCALAPPDATA%\M1-Panner
     * - Linux: ~/.cache/M1-Panner
     */
    static juce::File getStreamDirectory() {
        juce::File streamDir;
        
        #if JUCE_MAC
            // Priority 1: App Group container (for sandboxed plugins like AAX)
            // This path matches what M1MemoryShareTracker expects first
            juce::File appGroupDir = getAppGroupContainer("group.com.mach1.spatial.shared");
            if (appGroupDir.isDirectory() || appGroupDir.createDirectory()) {
                streamDir = appGroupDir.getChildFile("Library/Caches/M1-Panner");
                if (streamDir.createDirectory()) {
                    DBG("[StreamingMode] Using App Group container: " + streamDir.getFullPathName());
                    return streamDir;
                }
            }
            
            // Priority 2: User Library/Caches (non-sandboxed plugins)
            const char* homeDir = std::getenv("HOME");
            if (homeDir) {
                streamDir = juce::File(juce::String(homeDir) + "/Library/Caches/M1-Panner");
                if (streamDir.createDirectory()) {
                    DBG("[StreamingMode] Using user caches: " + streamDir.getFullPathName());
                    return streamDir;
                }
            }
            
            // Priority 3: /tmp/M1-Panner (fallback, always writable)
            streamDir = juce::File("/tmp/M1-Panner");
            streamDir.createDirectory();
            DBG("[StreamingMode] Using temp directory: " + streamDir.getFullPathName());
            
        #elif JUCE_WINDOWS
            // Windows: %LOCALAPPDATA%\M1-Panner
            const char* localAppData = std::getenv("LOCALAPPDATA");
            if (localAppData) {
                streamDir = juce::File(juce::String(localAppData) + "\\M1-Panner");
            } else {
                streamDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("M1-Panner");
            }
            streamDir.createDirectory();
            
        #else
            // Linux: ~/.cache/M1-Panner
            const char* homeDir = std::getenv("HOME");
            if (homeDir) {
                streamDir = juce::File(juce::String(homeDir) + "/.cache/M1-Panner");
            } else {
                streamDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(".cache/M1-Panner");
            }
            
            if (!streamDir.createDirectory()) {
                // Fallback to /tmp
                streamDir = juce::File("/tmp/M1-Panner");
                streamDir.createDirectory();
            }
        #endif
        
        return streamDir;
    }
    
    /**
     * Get App Group container on macOS (for sandboxed plugins like AAX)
     * Returns empty File if not available or not on macOS
     */
    static juce::File getAppGroupContainer(const juce::String& groupIdentifier) {
        #if JUCE_MAC
            // Use NSFileManager to get the App Group container
            // This is available to both sandboxed and non-sandboxed apps
            juce::File containerDir;
            
            // Try to find existing group container by checking common location
            const char* homeDir = std::getenv("HOME");
            if (homeDir) {
                juce::String groupPath = juce::String(homeDir) + "/Library/Group Containers/" + groupIdentifier;
                containerDir = juce::File(groupPath);
                
                // If it doesn't exist, try to create it (will only work if app is entitled)
                if (!containerDir.exists()) {
                    containerDir.createDirectory();
                }
                
                if (containerDir.exists()) {
                    DBG("[StreamingMode] App Group container: " + containerDir.getFullPathName());
                    return containerDir;
                }
            }
            
            DBG("[StreamingMode] App Group container not available for: " + groupIdentifier);
        #else
            juce::ignoreUnused(groupIdentifier);
        #endif
        
        return juce::File();
    }
    
    /**
     * Generate unique instance name for memory share file
     * Format: M1SpatialSystem_M1Panner_PID{pid}_PTR{ptr}_T{timestamp}
     * 
     * This matches the format expected by M1MemoryShareTracker in m1-system-helper
     * and is used for both file naming and helper service registration.
     * 
     * @param instancePtr Pointer to the panner instance (for PTR field)
     * @return Unique instance name string
     */
    static juce::String generateUniqueInstanceName(const void* instancePtr) {
        #if JUCE_WINDOWS
            uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
        #else
            uint32_t pid = static_cast<uint32_t>(getpid());
        #endif
        
        uintptr_t ptr = reinterpret_cast<uintptr_t>(instancePtr);
        uint64_t timestamp = static_cast<uint64_t>(juce::Time::currentTimeMillis());
        
        return juce::String("M1SpatialSystem_M1Panner_PID") + juce::String(pid) + 
               "_PTR" + juce::String::toHexString(static_cast<juce::int64>(ptr)) + 
               "_T" + juce::String(timestamp);
    }
    
    /**
     * Generate platform-specific path for the memory-mapped file (public for debugging)
     * 
     * Filename format matches M1MemoryShareTracker expectations:
     * M1SpatialSystem_M1Panner_PID{pid}_PTR{ptr}_T{timestamp}.mem
     * 
     * @param instanceName The unique instance name (from generateUniqueInstanceName)
     * @return Full path to the memory file
     */
    static juce::String getStreamFilePath(const juce::String& instanceName) {
        juce::File streamDir = getStreamDirectory();
        
        juce::String filename = instanceName + ".mem";
        juce::String filePath = streamDir.getChildFile(filename).getFullPathName();
        
        DBG("[StreamingMode] Stream directory: " + streamDir.getFullPathName() + 
            " (exists: " + juce::String(streamDir.exists() ? "yes" : "no") + ")");
        DBG("[StreamingMode] Stream file path: " + filePath);
        
        return filePath;
    }
    
    /**
     * Get the unique instance name for this streaming instance
     */
    juce::String getInstanceName() const { return instanceName; }
    
private:
    // Ring buffer and memory mapping
    LockFreeRingBuffer ringBuffer;
    std::unique_ptr<juce::MemoryMappedFile> mappedFile;
    
    // Pre-allocated buffer for channel pointers (sized in prepareStreaming)
    std::vector<const float*> tempChannelPointers;
    
    // Stream configuration
    juce::String instanceName;
    juce::String sessionId;
    juce::String mmapPath;
    uint32_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint32_t maxBlockSize = 0;
    
    // Thread-safe state flag
    std::atomic<bool> streamingPrepared{false};
};

/**
 * HelperAutoLauncher
 * 
 * Cross-platform utility to detect if m1-system-helper is running
 * and auto-launch it if needed.
 */
class HelperAutoLauncher {
public:
    /**
     * Check if m1-system-helper is reachable via OSC
     * Uses a quick UDP probe rather than full connection
     */
    static bool isHelperReachable(int helperPort) {
        // Try to bind to helper port - if it fails, helper is running
        juce::DatagramSocket socket(false);
        socket.setEnablePortReuse(false);
        bool bound = socket.bindToPort(helperPort);
        socket.shutdown();
        return !bound;  // If we couldn't bind, port is in use (helper running)
    }
    
    /**
     * Get the path to m1-system-helper executable
     */
    static juce::File getHelperPath() {
        juce::File helperPath;
        
        #if JUCE_MAC
            // macOS: Look in /Applications or adjacent to plugin
            helperPath = juce::File("/Applications/Mach1 Spatial System/m1-system-helper.app/Contents/MacOS/m1-system-helper");
            if (!helperPath.exists()) {
                // Try Application Support
                helperPath = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                    .getChildFile("Application Support/Mach1/m1-system-helper.app/Contents/MacOS/m1-system-helper");
            }
        #elif JUCE_WINDOWS
            // Windows: Look in Program Files
            helperPath = juce::File("C:/Program Files/Mach1 Spatial System/m1-system-helper.exe");
            if (!helperPath.exists()) {
                helperPath = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                    .getChildFile("Mach1/m1-system-helper.exe");
            }
        #else
            // Linux: Look in standard locations
            helperPath = juce::File("/usr/local/bin/m1-system-helper");
            if (!helperPath.exists()) {
                helperPath = juce::File("/opt/mach1/m1-system-helper");
            }
        #endif
        
        return helperPath;
    }
    
    /**
     * Launch m1-system-helper asynchronously (non-blocking)
     * Returns immediately, launch happens in background
     */
    static void launchHelperAsync() {
        // Use a static flag to prevent multiple launch attempts
        static std::atomic<bool> launchInProgress{false};
        static std::atomic<int64_t> lastLaunchAttempt{0};
        
        // Rate limit: don't try more than once every 5 seconds
        int64_t now = juce::Time::currentTimeMillis();
        if (now - lastLaunchAttempt.load() < 5000) {
            return;
        }
        
        if (launchInProgress.exchange(true)) {
            return;  // Already launching
        }
        
        lastLaunchAttempt.store(now);
        
        // Launch in background thread
        std::thread([]() {
            juce::File helperPath = getHelperPath();
            
            if (helperPath.exists()) {
                DBG("[HelperLauncher] Launching: " + helperPath.getFullPathName());
                
                #if JUCE_MAC
                    // Use open command for macOS .app bundles
                    juce::File appBundle = helperPath.getParentDirectory().getParentDirectory().getParentDirectory();
                    juce::String launchPath;
                    if (appBundle.getFileExtension() == ".app") {
                        launchPath = appBundle.getFullPathName();
                    } else {
                        launchPath = helperPath.getFullPathName();
                    }
                    // Use system() for fire-and-forget launch on macOS
                    std::string cmd = "open \"" + launchPath.toStdString() + "\" &";
                    std::system(cmd.c_str());
                #elif JUCE_WINDOWS
                    // Use ShellExecute equivalent via JUCE
                    juce::URL("file://" + helperPath.getFullPathName()).launchInDefaultBrowser();
                #else
                    // Linux - use system() for fire-and-forget launch
                    std::string cmd = "\"" + helperPath.getFullPathName().toStdString() + "\" &";
                    std::system(cmd.c_str());
                #endif
                
                // Wait a bit for helper to start
                juce::Thread::sleep(1000);
            } else {
                DBG("[HelperLauncher] Helper not found at: " + helperPath.getFullPathName());
            }
            
            launchInProgress.store(false);
        }).detach();
    }
};

} // namespace Mach1

