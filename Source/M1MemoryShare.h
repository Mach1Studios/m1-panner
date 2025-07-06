#pragma once

#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <cstring>

// Platform-specific includes for process ID
#if JUCE_WINDOWS
    #include <windows.h>
#elif JUCE_MAC || JUCE_LINUX || JUCE_IOS || JUCE_ANDROID
    #include <unistd.h>
#endif

/**
 * Structure for each audio buffer header containing panner settings and DAW timestamp
 */
struct AudioBufferHeader
{
    uint32_t channels;                      // Number of audio channels
    uint32_t samples;                       // Number of samples per channel
    uint64_t dawTimestamp;                  // DAW/host timestamp in milliseconds
    double playheadPositionInSeconds;       // DAW playhead position
    uint32_t isPlaying;                     // Is DAW playing (1) or stopped (0)

    // Panner settings - matching PannerSettings structure
    float azimuth;                          // Azimuth angle in degrees
    float elevation;                        // Elevation angle in degrees
    float diverge;                          // Divergence value
    float gain;                             // Gain in dB
    float stereoOrbitAzimuth;               // Stereo orbit azimuth
    float stereoSpread;                     // Stereo spread
    float stereoInputBalance;               // Stereo input balance
    uint32_t autoOrbit;                     // Auto orbit flag (1 = true, 0 = false)
    uint32_t isotropicMode;                 // Isotropic mode flag
    uint32_t equalpowerMode;                // Equal power mode flag
    uint32_t gainCompensationMode;          // Gain compensation mode flag
    uint32_t inputMode;                     // Mach1EncodeInputMode
    uint32_t outputMode;                    // Mach1EncodeOutputMode

    // Reserved for future expansion
    uint32_t reserved[8];

    AudioBufferHeader() : channels(0), samples(0), dawTimestamp(0), playheadPositionInSeconds(0.0),
                         isPlaying(0), azimuth(0.0f), elevation(0.0f), diverge(50.0f), gain(0.0f),
                         stereoOrbitAzimuth(0.0f), stereoSpread(50.0f), stereoInputBalance(0.0f),
                         autoOrbit(1), isotropicMode(0), equalpowerMode(0), gainCompensationMode(1),
                         inputMode(0), outputMode(0)
    {
        std::memset(reserved, 0, sizeof(reserved));
    }
};

/**
 * M1MemoryShare provides IPC (Inter-Process Communication) memory sharing functionality
 * using JUCE's MemoryMappedFile for sharing audio data and control data between processes.
 *
 * This class creates a shared memory segment that can be accessed by multiple processes
 * and provides thread-safe reading and writing operations.
 */
class M1MemoryShare
{
public:
    /**
     * Header structure that prefixes all shared memory segments
     * Contains metadata about the shared data
     */
    struct SharedMemoryHeader
    {
        volatile uint32_t writeIndex;           // Current write position
        volatile uint32_t readIndex;            // Current read position
        volatile uint32_t dataSize;             // Size of valid data
        volatile bool hasData;                  // Flag indicating if data is available
        uint32_t bufferSize;                    // Total buffer size (set once)
        uint32_t sampleRate;                    // Audio sample rate
        uint32_t numChannels;                   // Number of audio channels
        uint32_t samplesPerBlock;               // Samples per processing block
        char name[64];                          // Name identifier for debugging

        SharedMemoryHeader() : writeIndex(0), readIndex(0), dataSize(0), hasData(false),
                              bufferSize(0), sampleRate(0), numChannels(0), samplesPerBlock(0)
        {
            std::memset(name, 0, sizeof(name));
        }
    };

    /**
     * Constructor for creating/opening a shared memory segment
     * @param memoryName Unique name for the shared memory segment (OS-wide)
     * @param totalSize Total size of the shared memory in bytes (must be >= 4KB)
     * @param persistent If true, memory persists until manually deleted; if false, cleaned up when process exits
     * @param createMode If true, creates new segment; if false, opens existing segment
     */
    M1MemoryShare(const juce::String& memoryName,
                  size_t totalSize,
                  bool persistent = true,
                  bool createMode = true);

    ~M1MemoryShare();

    /**
     * Initialize the shared memory for audio data
     * @param sampleRate Audio sample rate
     * @param numChannels Number of audio channels
     * @param samplesPerBlock Samples per processing block
     * @return true if successful
     */
    bool initializeForAudio(uint32_t sampleRate, uint32_t numChannels, uint32_t samplesPerBlock);

    /**
     * Write audio buffer to shared memory with enhanced header containing panner settings and DAW timestamp
     * @param audioBuffer JUCE AudioBuffer containing the audio data
     * @param pannerSettings Current panner settings to include in header
     * @param dawTimestamp DAW/host timestamp
     * @param playheadPositionInSeconds DAW playhead position
     * @param isPlaying Whether DAW is currently playing
     * @return true if write was successful
     */
    bool writeAudioBufferWithSettings(const juce::AudioBuffer<float>& audioBuffer,
                                    struct PannerSettings& pannerSettings,
                                    uint64_t dawTimestamp,
                                    double playheadPositionInSeconds,
                                    bool isPlaying);

    /**
     * Write audio buffer to shared memory (legacy method for backward compatibility)
     * @param audioBuffer JUCE AudioBuffer containing the audio data
     * @return true if write was successful
     */
    bool writeAudioBuffer(const juce::AudioBuffer<float>& audioBuffer);

    /**
     * Read audio buffer from shared memory with enhanced header
     * @param audioBuffer JUCE AudioBuffer to store the read data
     * @param bufferHeader Output parameter to store the buffer header info
     * @return true if read was successful and data was available
     */
    bool readAudioBufferWithSettings(juce::AudioBuffer<float>& audioBuffer, AudioBufferHeader& bufferHeader);

    /**
     * Read only the header settings from shared memory (without audio data)
     * @param bufferHeader Output parameter to store the buffer header info
     * @return true if read was successful and header data was available
     */
    bool readHeaderSettings(AudioBufferHeader& bufferHeader);

    /**
     * Read audio buffer from shared memory (legacy method)
     * @param audioBuffer JUCE AudioBuffer to store the read data
     * @return true if read was successful and data was available
     */
    bool readAudioBuffer(juce::AudioBuffer<float>& audioBuffer);

    /**
     * Write string data to shared memory
     * @param data String to write
     * @return true if write was successful
     */
    bool writeString(const juce::String& data);

    /**
     * Read string data from shared memory
     * @return String data if available, empty string otherwise
     */
    juce::String readString();

    /**
     * Write raw binary data to shared memory
     * @param data Pointer to data
     * @param size Size of data in bytes
     * @return true if write was successful
     */
    bool writeData(const void* data, size_t size);

    /**
     * Read raw binary data from shared memory
     * @param buffer Buffer to store data
     * @param maxSize Maximum size to read
     * @return Number of bytes actually read
     */
    size_t readData(void* buffer, size_t maxSize);

    /**
     * Check if the shared memory is valid and accessible
     * @return true if memory is accessible
     */
    bool isValid() const;

    /**
     * Get the current data size in the shared memory
     * @return Size of available data in bytes
     */
    size_t getDataSize() const;

    /**
     * Clear all data in the shared memory
     */
    void clear();

    /**
     * Get memory usage statistics
     */
    struct MemoryStats
    {
        size_t totalSize;
        size_t availableSize;
        size_t usedSize;
        uint32_t writeCount;
        uint32_t readCount;
    };

    MemoryStats getStats() const;

    /**
     * Static method to delete a shared memory segment by name
     * @param memoryName Name of the memory segment to delete
     * @return true if successfully deleted
     */
    static bool deleteSharedMemory(const juce::String& memoryName);

private:
    juce::String m_memoryName;
    size_t m_totalSize;
    bool m_persistent;
    bool m_createMode;

    std::unique_ptr<juce::MemoryMappedFile> m_mappedFile;
    juce::File m_tempFile;

    SharedMemoryHeader* m_header;
    uint8_t* m_dataBuffer;
    size_t m_dataBufferSize;

    mutable std::atomic<uint32_t> m_writeCount{0};
    mutable std::atomic<uint32_t> m_readCount{0};

    bool createSharedMemoryFile();
    bool openSharedMemoryFile();
    void setupMemoryPointers();

    // Prevent copying
    M1MemoryShare(const M1MemoryShare&) = delete;
    M1MemoryShare& operator=(const M1MemoryShare&) = delete;
};
