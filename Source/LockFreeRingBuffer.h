#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <algorithm>

/**
 * Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer
 * 
 * Designed for real-time audio streaming between processes via memory-mapped files.
 * 
 * Features:
 * - No locks, no allocations in read/write paths
 * - Cache-line padding to prevent false sharing
 * - Supports multichannel audio with interleaved storage
 * - Includes metadata header for synchronization
 * 
 * Memory Layout:
 * [StreamHeader][padding][audio samples...]
 */

namespace Mach1 {

// Cache line size for padding (typical x86/ARM)
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * Stream header stored at the beginning of shared memory
 * Aligned and padded to prevent false sharing between producer/consumer
 */
struct alignas(CACHE_LINE_SIZE) StreamHeader {
    // Magic number to verify valid stream
    static constexpr uint32_t MAGIC = 0x4D315354; // "M1ST"
    static constexpr uint32_t VERSION = 1;
    
    // Immutable after initialization
    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t numChannels = 0;
    uint32_t bufferCapacitySamples = 0;  // Per-channel capacity in samples
    uint32_t sampleRate = 0;
    uint32_t maxBlockSize = 0;
    
    // Panner identification
    char pannerUuid[40] = {0};    // UUID string (36 chars + null)
    char sessionId[24] = {0};     // Host PID as string
    
    // Mach1Encode coefficients (up to 14 channels output, 2 input channels for stereo)
    // Layout: gains[inputChannel][outputChannel]
    // For stereo input -> 14ch output: 2 * 14 = 28 floats
    float encodeGains[2][14] = {{0}};
    uint32_t encodeInputMode = 0;
    uint32_t encodeOutputMode = 0;
    uint32_t encodePannerMode = 0;
    
    // Spatial parameters (for UI sync)
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float diverge = 0.0f;
    float gain = 0.0f;  // dB
    
    // Stereo parameters
    uint32_t autoOrbit = 1;
    float stereoOrbitAzimuth = 0.0f;
    float stereoSpread = 0.5f;
    float stereoInputBalance = 0.0f;
    
    // Transport info
    double playheadPositionSeconds = 0.0;
    uint32_t isPlaying = 0;
    uint64_t dawTimestamp = 0;
    
    // Stream state
    uint32_t streamActive = 0;
    uint64_t lastWriteTimestamp = 0;
    
    // Padding to separate from indices
    char _pad1[CACHE_LINE_SIZE - (sizeof(uint32_t) * 2 + sizeof(uint64_t) * 2) % CACHE_LINE_SIZE];
    
    // Producer-owned index (written by producer, read by consumer)
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> writeIndex{0};
    char _pad2[CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>)];
    
    // Consumer-owned index (written by consumer, read by producer)
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> readIndex{0};
    char _pad3[CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>)];
    
    // Sequence number for detecting overwrites
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> sequenceNumber{0};
};

/**
 * Lock-free SPSC Ring Buffer for multichannel audio
 * 
 * Audio is stored interleaved: [ch0_s0, ch1_s0, ch0_s1, ch1_s1, ...]
 */
class LockFreeRingBuffer {
public:
    LockFreeRingBuffer() = default;
    ~LockFreeRingBuffer() = default;
    
    /**
     * Initialize buffer with existing memory region
     * @param memory Pointer to memory-mapped region
     * @param totalBytes Total size of memory region
     * @param numChannels Number of audio channels
     * @param capacitySamples Per-channel buffer capacity in samples
     * @param sampleRate Audio sample rate
     * @param maxBlockSize Maximum expected block size
     * @param initializeHeader If true, initialize header (producer). If false, just attach (consumer)
     * @return true if initialization successful
     */
    bool initialize(void* memory, size_t totalBytes, uint32_t numChannels,
                   uint32_t capacitySamples, uint32_t sampleRate, uint32_t maxBlockSize,
                   bool initializeHeader) {
        if (!memory || totalBytes < getRequiredSize(numChannels, capacitySamples)) {
            return false;
        }
        
        header = reinterpret_cast<StreamHeader*>(memory);
        audioData = reinterpret_cast<float*>(
            reinterpret_cast<uint8_t*>(memory) + sizeof(StreamHeader)
        );
        
        this->numChannels = numChannels;
        this->capacitySamples = capacitySamples;
        
        if (initializeHeader) {
            // Producer initializes the header
            std::memset(header, 0, sizeof(StreamHeader));
            header->magic = StreamHeader::MAGIC;
            header->version = StreamHeader::VERSION;
            header->numChannels = numChannels;
            header->bufferCapacitySamples = capacitySamples;
            header->sampleRate = sampleRate;
            header->maxBlockSize = maxBlockSize;
            header->writeIndex.store(0, std::memory_order_relaxed);
            header->readIndex.store(0, std::memory_order_relaxed);
            header->sequenceNumber.store(0, std::memory_order_relaxed);
            header->streamActive = 1;
            
            // Zero audio buffer
            std::memset(audioData, 0, numChannels * capacitySamples * sizeof(float));
        } else {
            // Consumer verifies header
            if (header->magic != StreamHeader::MAGIC || header->version != StreamHeader::VERSION) {
                return false;
            }
        }
        
        initialized = true;
        return true;
    }
    
    /**
     * Attach to existing initialized buffer (consumer side)
     */
    bool attach(void* memory, size_t totalBytes) {
        if (!memory || totalBytes < sizeof(StreamHeader)) {
            return false;
        }
        
        header = reinterpret_cast<StreamHeader*>(memory);
        
        // Verify magic and version
        if (header->magic != StreamHeader::MAGIC || header->version != StreamHeader::VERSION) {
            return false;
        }
        
        numChannels = header->numChannels;
        capacitySamples = header->bufferCapacitySamples;
        
        if (totalBytes < getRequiredSize(numChannels, capacitySamples)) {
            return false;
        }
        
        audioData = reinterpret_cast<float*>(
            reinterpret_cast<uint8_t*>(memory) + sizeof(StreamHeader)
        );
        
        initialized = true;
        return true;
    }
    
    /**
     * Write multichannel audio block (producer only)
     * Non-blocking, overwrites oldest data if buffer is full
     * 
     * @param channelData Array of channel pointers [numChannels][numSamples]
     * @param numSamples Number of samples to write per channel
     * @return true if write successful
     */
    bool write(const float* const* channelData, uint32_t numSamples) {
        if (!initialized || !header || !audioData || numSamples == 0) {
            return false;
        }
        
        const uint64_t writeIdx = header->writeIndex.load(std::memory_order_relaxed);
        
        // Write interleaved samples
        for (uint32_t s = 0; s < numSamples; ++s) {
            const uint64_t ringPos = (writeIdx + s) % capacitySamples;
            const size_t baseIdx = ringPos * numChannels;
            
            for (uint32_t ch = 0; ch < numChannels; ++ch) {
                audioData[baseIdx + ch] = channelData[ch][s];
            }
        }
        
        // Update write index with release semantics (ensures audio data is visible)
        header->writeIndex.store(writeIdx + numSamples, std::memory_order_release);
        header->sequenceNumber.fetch_add(1, std::memory_order_relaxed);
        
        return true;
    }
    
    /**
     * Write single-channel audio and duplicate to all channels (producer only)
     */
    bool writeMono(const float* monoData, uint32_t numSamples) {
        if (!initialized || !header || !audioData || numSamples == 0) {
            return false;
        }
        
        const uint64_t writeIdx = header->writeIndex.load(std::memory_order_relaxed);
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            const uint64_t ringPos = (writeIdx + s) % capacitySamples;
            const size_t baseIdx = ringPos * numChannels;
            const float sample = monoData[s];
            
            for (uint32_t ch = 0; ch < numChannels; ++ch) {
                audioData[baseIdx + ch] = sample;
            }
        }
        
        header->writeIndex.store(writeIdx + numSamples, std::memory_order_release);
        header->sequenceNumber.fetch_add(1, std::memory_order_relaxed);
        
        return true;
    }
    
    /**
     * Read multichannel audio block (consumer only)
     * Non-blocking, returns available samples up to numSamples
     * 
     * @param channelData Array of channel pointers [numChannels][numSamples]
     * @param numSamples Maximum samples to read per channel
     * @return Number of samples actually read per channel
     */
    uint32_t read(float* const* channelData, uint32_t numSamples) {
        if (!initialized || !header || !audioData || numSamples == 0) {
            return 0;
        }
        
        const uint64_t readIdx = header->readIndex.load(std::memory_order_relaxed);
        const uint64_t writeIdx = header->writeIndex.load(std::memory_order_acquire);
        
        // Calculate available samples
        const uint64_t available = writeIdx - readIdx;
        const uint32_t toRead = static_cast<uint32_t>(std::min(static_cast<uint64_t>(numSamples), available));
        
        if (toRead == 0) {
            return 0;
        }
        
        // Read interleaved samples
        for (uint32_t s = 0; s < toRead; ++s) {
            const uint64_t ringPos = (readIdx + s) % capacitySamples;
            const size_t baseIdx = ringPos * numChannels;
            
            for (uint32_t ch = 0; ch < numChannels; ++ch) {
                channelData[ch][s] = audioData[baseIdx + ch];
            }
        }
        
        // Update read index
        header->readIndex.store(readIdx + toRead, std::memory_order_release);
        
        return toRead;
    }
    
    /**
     * Get number of samples available to read
     */
    uint64_t availableToRead() const {
        if (!initialized || !header) return 0;
        const uint64_t readIdx = header->readIndex.load(std::memory_order_relaxed);
        const uint64_t writeIdx = header->writeIndex.load(std::memory_order_acquire);
        return writeIdx - readIdx;
    }
    
    /**
     * Get number of samples available to write before overwriting unread data
     */
    uint64_t availableToWrite() const {
        if (!initialized || !header) return 0;
        const uint64_t readIdx = header->readIndex.load(std::memory_order_acquire);
        const uint64_t writeIdx = header->writeIndex.load(std::memory_order_relaxed);
        return capacitySamples - (writeIdx - readIdx);
    }
    
    /**
     * Calculate required memory size for given configuration
     */
    static size_t getRequiredSize(uint32_t numChannels, uint32_t capacitySamples) {
        return sizeof(StreamHeader) + (numChannels * capacitySamples * sizeof(float));
    }
    
    /**
     * Access header for updating metadata (producer only)
     */
    StreamHeader* getHeader() { return header; }
    const StreamHeader* getHeader() const { return header; }
    
    bool isInitialized() const { return initialized; }
    uint32_t getNumChannels() const { return numChannels; }
    uint32_t getCapacitySamples() const { return capacitySamples; }
    
    /**
     * Mark stream as inactive (producer cleanup)
     */
    void deactivate() {
        if (header) {
            header->streamActive = 0;
        }
    }
    
    /**
     * Check if stream is active
     */
    bool isStreamActive() const {
        return header && header->streamActive != 0;
    }

private:
    StreamHeader* header = nullptr;
    float* audioData = nullptr;
    uint32_t numChannels = 0;
    uint32_t capacitySamples = 0;
    bool initialized = false;
};

} // namespace Mach1

