#include "M1MemoryShare.h"
#include <iostream>

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