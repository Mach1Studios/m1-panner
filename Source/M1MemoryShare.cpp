#include "M1MemoryShare.h"
#include "TypesForDataExchange.h"
#include <iostream>
#include <cstring>

//==============================================================================
M1MemoryShare::M1MemoryShare(const juce::String& memoryName,
                             size_t totalSize,
                             bool persistent,
                             bool createMode)
    : m_memoryName(memoryName)
    , m_totalSize(totalSize)
    , m_persistent(persistent)
    , m_createMode(createMode)
    , m_header(nullptr)
    , m_dataBuffer(nullptr)
    , m_dataBufferSize(0)
{
    // Ensure minimum size for header
    if (m_totalSize < sizeof(SharedMemoryHeader) + 1024)
    {
        m_totalSize = sizeof(SharedMemoryHeader) + 1024;
    }

    // Create or open the shared memory
    if (m_createMode)
    {
        if (!createSharedMemoryFile())
        {
            DBG("[M1MemoryShare] Failed to create shared memory: " + m_memoryName);
            return;
        }
    }
    else
    {
        if (!openSharedMemoryFile())
        {
            DBG("[M1MemoryShare] Failed to open shared memory: " + m_memoryName);
            return;
        }
    }

    setupMemoryPointers();
}

M1MemoryShare::~M1MemoryShare()
{
    if (m_mappedFile)
    {
        m_mappedFile.reset();
    }

    // Clean up temporary file if not persistent
    if (!m_persistent && m_tempFile.exists())
    {
        m_tempFile.deleteFile();
    }
}

//==============================================================================
bool M1MemoryShare::createSharedMemoryFile()
{
    // Create a temporary file for the shared memory
    juce::String tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName();
    m_tempFile = juce::File(tempDir + "/M1SpatialSystem_" + m_memoryName + ".mem");

    DBG("[M1MemoryShare] Attempting to create file: " + m_tempFile.getFullPathName());
    DBG("[M1MemoryShare] Total size to allocate: " + juce::String(m_totalSize) + " bytes");

    // Create the file with the required size
    juce::FileOutputStream outputStream(m_tempFile);
    if (!outputStream.openedOk())
    {
        DBG("[M1MemoryShare] Failed to create temp file for shared memory: " + m_tempFile.getFullPathName());
        DBG("[M1MemoryShare] Temp directory: " + tempDir);
        return false;
    }

    // Write zeros to create a file of the required size
    std::vector<char> buffer(8192, 0);
    size_t bytesWritten = 0;
    while (bytesWritten < m_totalSize)
    {
        size_t bytesToWrite = std::min(buffer.size(), m_totalSize - bytesWritten);
        outputStream.write(buffer.data(), bytesToWrite);
        bytesWritten += bytesToWrite;
    }

    outputStream.flush();

    // Map the file into memory
    DBG("[M1MemoryShare] File created successfully, attempting to map into memory");
    m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile,
                                                           juce::MemoryMappedFile::readWrite,
                                                           false);

    if (!m_mappedFile->getData())
    {
        DBG("[M1MemoryShare] Failed to map shared memory file: " + m_tempFile.getFullPathName());
        DBG("[M1MemoryShare] File exists: " + juce::String(m_tempFile.exists() ? "Yes" : "No"));
        DBG("[M1MemoryShare] File size: " + juce::String(m_tempFile.getSize()) + " bytes");
        return false;
    }

    DBG("[M1MemoryShare] Successfully created and mapped shared memory file: " + m_tempFile.getFullPathName());
    return true;
}

bool M1MemoryShare::openSharedMemoryFile()
{
    // Try to open existing file
    juce::String tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName();
    m_tempFile = juce::File(tempDir + "/M1SpatialSystem_" + m_memoryName + ".mem");

    if (!m_tempFile.exists())
    {
        DBG("[M1MemoryShare] Shared memory file does not exist: " + m_tempFile.getFullPathName());
        return false;
    }

    // Map the existing file
    m_mappedFile = std::make_unique<juce::MemoryMappedFile>(m_tempFile,
                                                           juce::MemoryMappedFile::readWrite,
                                                           false);

    if (!m_mappedFile->getData())
    {
        DBG("[M1MemoryShare] Failed to map existing shared memory file");
        return false;
    }

    return true;
}

void M1MemoryShare::setupMemoryPointers()
{
    if (!m_mappedFile || !m_mappedFile->getData())
    {
        return;
    }

    // Set up header pointer
    m_header = static_cast<SharedMemoryHeader*>(m_mappedFile->getData());

    // Set up data buffer pointer (after header)
    m_dataBuffer = static_cast<uint8_t*>(m_mappedFile->getData()) + sizeof(SharedMemoryHeader);
    m_dataBufferSize = m_totalSize - sizeof(SharedMemoryHeader);

    // Initialize header if we're in create mode
    if (m_createMode)
    {
        *m_header = SharedMemoryHeader(); // Initialize with defaults
        m_header->bufferSize = static_cast<uint32_t>(m_dataBufferSize);
        strncpy(m_header->name, m_memoryName.toUTF8(), sizeof(m_header->name) - 1);
    }
}

//==============================================================================
bool M1MemoryShare::initializeForAudio(uint32_t sampleRate, uint32_t numChannels, uint32_t samplesPerBlock)
{
    if (!isValid())
    {
        return false;
    }

    m_header->sampleRate = sampleRate;
    m_header->numChannels = numChannels;
    m_header->samplesPerBlock = samplesPerBlock;

    // Clear any existing data
    clear();

    return true;
}

bool M1MemoryShare::writeAudioBufferWithSettings(const juce::AudioBuffer<float>& audioBuffer,
                                                PannerSettings& pannerSettings,
                                                uint64_t dawTimestamp,
                                                double playheadPositionInSeconds,
                                                bool isPlaying)
{
    if (!isValid())
    {
        DBG("[M1MemoryShare] writeAudioBufferWithSettings: Not valid");
        return false;
    }

    // Calculate required size for the enhanced audio data
    size_t samplesCount = audioBuffer.getNumSamples();
    size_t channelsCount = audioBuffer.getNumChannels();
    size_t audioDataSize = samplesCount * channelsCount * sizeof(float);

    // Add enhanced header info: AudioBufferHeader + audio data
    size_t totalSize = sizeof(AudioBufferHeader) + audioDataSize;

    if (totalSize > m_dataBufferSize)
    {
        DBG("[M1MemoryShare] Audio buffer with settings too large for shared memory: " +
            juce::String(totalSize) + " > " + juce::String(m_dataBufferSize));
        return false;
    }

    // Write the enhanced header
    uint8_t* writePtr = m_dataBuffer;
    AudioBufferHeader* bufferHeader = reinterpret_cast<AudioBufferHeader*>(writePtr);

    // Fill in the enhanced header
    bufferHeader->channels = static_cast<uint32_t>(channelsCount);
    bufferHeader->samples = static_cast<uint32_t>(samplesCount);
    bufferHeader->dawTimestamp = dawTimestamp;
    bufferHeader->playheadPositionInSeconds = playheadPositionInSeconds;
    bufferHeader->isPlaying = isPlaying ? 1 : 0;

    // Copy panner settings
    bufferHeader->azimuth = pannerSettings.azimuth;
    bufferHeader->elevation = pannerSettings.elevation;
    bufferHeader->diverge = pannerSettings.diverge;
    bufferHeader->gain = pannerSettings.gain;
    bufferHeader->autoOrbit = pannerSettings.autoOrbit ? 1 : 0;
    bufferHeader->stereoOrbitAzimuth = pannerSettings.stereoOrbitAzimuth;
    bufferHeader->stereoSpread = pannerSettings.stereoSpread;
    bufferHeader->stereoInputBalance = pannerSettings.stereoInputBalance;
    bufferHeader->isotropicMode = pannerSettings.isotropicMode ? 1 : 0;
    bufferHeader->equalpowerMode = pannerSettings.equalpowerMode ? 1 : 0;
    bufferHeader->gainCompensationMode = pannerSettings.gainCompensationMode ? 1 : 0;
    bufferHeader->inputMode = static_cast<uint32_t>(pannerSettings.m1Encode.getInputMode());
    bufferHeader->outputMode = static_cast<uint32_t>(pannerSettings.m1Encode.getOutputMode());

    writePtr += sizeof(AudioBufferHeader);

    // Write audio data (interleaved) using efficient getReadPointer access
    float* audioDataPtr = reinterpret_cast<float*>(writePtr);

    // Get read pointers for each channel once (more efficient)
    const float* channelPointers[2];  // Max 2 channels should be enough
    for (int channel = 0; channel < channelsCount; ++channel)
    {
        channelPointers[channel] = audioBuffer.getReadPointer(channel);
    }

    // Write interleaved audio data
    for (int sample = 0; sample < samplesCount; ++sample)
    {
        for (int channel = 0; channel < channelsCount; ++channel)
        {
            audioDataPtr[sample * channelsCount + channel] = channelPointers[channel][sample];
        }
    }

    // Calculate audio levels for debugging
    float maxLevel = 0.0f;
    float sumSquares = 0.0f;
    for (int sample = 0; sample < samplesCount; ++sample)
    {
        for (int channel = 0; channel < channelsCount; ++channel)
        {
            float sampleValue = audioDataPtr[sample * channelsCount + channel];
            maxLevel = std::max(maxLevel, std::abs(sampleValue));
            sumSquares += sampleValue * sampleValue;
        }
    }
    float rmsLevel = std::sqrt(sumSquares / (samplesCount * channelsCount));

    // Update main header
    m_header->dataSize = static_cast<uint32_t>(totalSize);
    m_header->hasData = true;
    ++m_writeCount;

    DBG("[M1MemoryShare] Successfully wrote audio buffer with settings: " +
        juce::String(channelsCount) + " channels, " + juce::String(samplesCount) + " samples, " +
        "maxLevel=" + juce::String(maxLevel, 6) + ", rmsLevel=" + juce::String(rmsLevel, 6) + ", " +
        "azimuth=" + juce::String(pannerSettings.azimuth) + ", elevation=" + juce::String(pannerSettings.elevation));

    return true;
}

bool M1MemoryShare::readAudioBufferWithSettings(juce::AudioBuffer<float>& audioBuffer, AudioBufferHeader& bufferHeader)
{
    if (!isValid() || !m_header->hasData)
    {
        return false;
    }

    if (m_header->dataSize < sizeof(AudioBufferHeader))
    {
        DBG("[M1MemoryShare] Data size too small for enhanced header");
        return false;
    }

    const uint8_t* readPtr = m_dataBuffer;

    // Read the enhanced header
    const AudioBufferHeader* header = reinterpret_cast<const AudioBufferHeader*>(readPtr);
    bufferHeader = *header;  // Copy the header data

    uint32_t channelsCount = header->channels;
    uint32_t samplesCount = header->samples;

    // Validation
    if (channelsCount > 32 || samplesCount > 65536)  // Reasonable limits
    {
        DBG("[M1MemoryShare] Invalid audio data dimensions: " +
            juce::String(channelsCount) + " channels, " + juce::String(samplesCount) + " samples");
        return false;
    }

    readPtr += sizeof(AudioBufferHeader);

    // Ensure audio buffer has correct size
    audioBuffer.setSize(channelsCount, samplesCount, false, true, true);

    // Read audio data (interleaved)
    const float* audioDataPtr = reinterpret_cast<const float*>(readPtr);
    for (int sample = 0; sample < samplesCount; ++sample)
    {
        for (int channel = 0; channel < channelsCount; ++channel)
        {
            audioBuffer.setSample(channel, sample, audioDataPtr[sample * channelsCount + channel]);
        }
    }

    ++m_readCount;
    return true;
}

bool M1MemoryShare::readHeaderSettings(AudioBufferHeader& bufferHeader)
{
    if (!isValid() || !m_header->hasData)
    {
        return false;
    }

    if (m_header->dataSize < sizeof(AudioBufferHeader))
    {
        DBG("[M1MemoryShare] Data size too small for enhanced header in readHeaderSettingsOnly");
        return false;
    }

    const uint8_t* readPtr = m_dataBuffer;

    // Read only the enhanced header (no audio data)
    const AudioBufferHeader* header = reinterpret_cast<const AudioBufferHeader*>(readPtr);
    bufferHeader = *header;  // Copy the header data

    // Basic validation of header data
    if (bufferHeader.channels > 256 || bufferHeader.samples > 65536)  // Reasonable limits
    {
        DBG("[M1MemoryShare] Invalid header data in readHeaderSettingsOnly: " +
            juce::String(bufferHeader.channels) + " channels, " + juce::String(bufferHeader.samples) + " samples");
        return false;
    }

    DBG("[M1MemoryShare] Successfully read header settings: azimuth=" + juce::String(bufferHeader.azimuth) +
        ", elevation=" + juce::String(bufferHeader.elevation) + ", diverge=" + juce::String(bufferHeader.diverge) +
        ", inputMode=" + juce::String(bufferHeader.inputMode) + ", outputMode=" + juce::String(bufferHeader.outputMode));

    ++m_readCount;
    return true;
}

bool M1MemoryShare::writeAudioBuffer(const juce::AudioBuffer<float>& audioBuffer)
{
    if (!isValid())
    {
        return false;
    }

    // Calculate required size for the audio data
    size_t samplesCount = audioBuffer.getNumSamples();
    size_t channelsCount = audioBuffer.getNumChannels();
    size_t audioDataSize = samplesCount * channelsCount * sizeof(float);

    // Add header info: channels, samples, data
    size_t totalSize = sizeof(uint32_t) * 2 + audioDataSize; // channels + samples + audio data

    if (totalSize > m_dataBufferSize)
    {
        DBG("[M1MemoryShare] Audio buffer too large for shared memory");
        return false;
    }

    // Write the data
    uint8_t* writePtr = m_dataBuffer;

    // Write number of channels
    *reinterpret_cast<uint32_t*>(writePtr) = static_cast<uint32_t>(channelsCount);
    writePtr += sizeof(uint32_t);

    // Write number of samples
    *reinterpret_cast<uint32_t*>(writePtr) = static_cast<uint32_t>(samplesCount);
    writePtr += sizeof(uint32_t);

    // Write audio data (interleaved)
    for (int sample = 0; sample < samplesCount; ++sample)
    {
        for (int channel = 0; channel < channelsCount; ++channel)
        {
            *reinterpret_cast<float*>(writePtr) = audioBuffer.getSample(channel, sample);
            writePtr += sizeof(float);
        }
    }

    // Update header
    m_header->dataSize = static_cast<uint32_t>(totalSize);
    m_header->hasData = true;
    ++m_writeCount;

    return true;
}

bool M1MemoryShare::readAudioBuffer(juce::AudioBuffer<float>& audioBuffer)
{
    if (!isValid() || !m_header->hasData)
    {
        return false;
    }

    const uint8_t* readPtr = m_dataBuffer;

    // Read number of channels
    uint32_t channelsCount = *reinterpret_cast<const uint32_t*>(readPtr);
    readPtr += sizeof(uint32_t);

    // Read number of samples
    uint32_t samplesCount = *reinterpret_cast<const uint32_t*>(readPtr);
    readPtr += sizeof(uint32_t);

    // Ensure audio buffer has correct size
    audioBuffer.setSize(channelsCount, samplesCount, false, true, true);

    // Read audio data (interleaved)
    for (int sample = 0; sample < samplesCount; ++sample)
    {
        for (int channel = 0; channel < channelsCount; ++channel)
        {
            audioBuffer.setSample(channel, sample, *reinterpret_cast<const float*>(readPtr));
            readPtr += sizeof(float);
        }
    }

    ++m_readCount;
    return true;
}

bool M1MemoryShare::writeString(const juce::String& data)
{
    if (!isValid())
    {
        return false;
    }

    juce::String utf8String = data.toUTF8();
    const char* utf8Data = utf8String.toUTF8();
    size_t dataSize = strlen(utf8Data) + 1; // +1 for null terminator

    if (dataSize > m_dataBufferSize)
    {
        DBG("[M1MemoryShare] String too large for shared memory");
        return false;
    }

    // Copy string data
    memcpy(m_dataBuffer, utf8Data, dataSize);

    // Update header
    m_header->dataSize = static_cast<uint32_t>(dataSize);
    m_header->hasData = true;
    ++m_writeCount;

    return true;
}

juce::String M1MemoryShare::readString()
{
    if (!isValid() || !m_header->hasData || m_header->dataSize == 0)
    {
        return juce::String();
    }

    // Ensure null termination
    char* stringData = reinterpret_cast<char*>(m_dataBuffer);
    size_t maxLen = std::min(static_cast<size_t>(m_header->dataSize), m_dataBufferSize - 1);
    stringData[maxLen] = '\0';

    ++m_readCount;
    return juce::String(stringData);
}

bool M1MemoryShare::writeData(const void* data, size_t size)
{
    if (!isValid() || size > m_dataBufferSize)
    {
        return false;
    }

    memcpy(m_dataBuffer, data, size);

    // Update header
    m_header->dataSize = static_cast<uint32_t>(size);
    m_header->hasData = true;
    ++m_writeCount;

    return true;
}

size_t M1MemoryShare::readData(void* buffer, size_t maxSize)
{
    if (!isValid() || !m_header->hasData)
    {
        return 0;
    }

    size_t bytesToRead = std::min(maxSize, static_cast<size_t>(m_header->dataSize));
    memcpy(buffer, m_dataBuffer, bytesToRead);

    ++m_readCount;
    return bytesToRead;
}

//==============================================================================
bool M1MemoryShare::isValid() const
{
    bool mappedFileValid = (m_mappedFile != nullptr);
    bool dataValid = (m_mappedFile && m_mappedFile->getData() != nullptr);
    bool headerValid = (m_header != nullptr);
    bool bufferValid = (m_dataBuffer != nullptr);

    bool result = mappedFileValid && dataValid && headerValid && bufferValid;

    if (!result)
    {
        DBG("[M1MemoryShare] isValid() failed - mappedFile: " + juce::String(mappedFileValid ? "OK" : "FAIL") +
            ", data: " + juce::String(dataValid ? "OK" : "FAIL") +
            ", header: " + juce::String(headerValid ? "OK" : "FAIL") +
            ", buffer: " + juce::String(bufferValid ? "OK" : "FAIL"));
    }

    return result;
}

size_t M1MemoryShare::getDataSize() const
{
    if (!isValid())
    {
        return 0;
    }

    return m_header->dataSize;
}

void M1MemoryShare::clear()
{
    if (!isValid())
    {
        return;
    }

    m_header->writeIndex = 0;
    m_header->readIndex = 0;
    m_header->dataSize = 0;
    m_header->hasData = false;

    // Clear data buffer
    memset(m_dataBuffer, 0, m_dataBufferSize);
}

M1MemoryShare::MemoryStats M1MemoryShare::getStats() const
{
    MemoryStats stats;

    if (!isValid())
    {
        stats.totalSize = 0;
        stats.availableSize = 0;
        stats.usedSize = 0;
        stats.writeCount = 0;
        stats.readCount = 0;
        return stats;
    }

    stats.totalSize = m_totalSize;
    stats.usedSize = m_header->dataSize;
    stats.availableSize = m_dataBufferSize - stats.usedSize;
    stats.writeCount = m_writeCount.load();
    stats.readCount = m_readCount.load();

    return stats;
}

//==============================================================================
bool M1MemoryShare::deleteSharedMemory(const juce::String& memoryName)
{
    juce::String tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName();
    juce::File memoryFile(tempDir + "/M1SpatialSystem_" + memoryName + ".mem");

    if (memoryFile.exists())
    {
        return memoryFile.deleteFile();
    }

    return true; // File doesn't exist, consider it deleted
}
