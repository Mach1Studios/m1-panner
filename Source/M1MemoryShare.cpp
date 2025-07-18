#include "M1MemoryShare.h"
#include "TypesForDataExchange.h"
#include "Utility/SharedMemoryPaths.h"
#include <iostream>
#include <cstring>
#include <filesystem>
#include <chrono>

#if JUCE_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

//==============================================================================
M1MemoryShare::M1MemoryShare(const juce::String& memoryName,
                             size_t totalSize,
                             uint32_t maxQueueSize,
                             bool persistent,
                             bool createMode)
    : m_memoryName(memoryName)
    , m_totalSize(totalSize)
    , m_maxQueueSize(maxQueueSize)
    , m_persistent(persistent)
    , m_createMode(createMode)
    , m_header(nullptr)
    , m_dataBuffer(nullptr)
    , m_dataBufferSize(0)
    , m_queuedBuffers(nullptr)
    , m_queuedBuffersSize(0)
{
    // Ensure minimum size for header and queue
    size_t minSize = sizeof(SharedMemoryHeader) +
                    (maxQueueSize * sizeof(QueuedBuffer)) +
                    1024; // minimum data buffer

    if (m_totalSize < minSize)
    {
        m_totalSize = minSize;
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
    // Ensure the shared memory directory exists before creating the file
    if (!Mach1::SharedMemoryPaths::ensureMemoryDirectoryExists()) {
        DBG("[M1MemoryShare] Failed to create or access shared memory directory");
        // Fallback to temp directory
        std::string tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString();
        m_tempFile = juce::File(juce::String(tempDir) + "/M1SpatialSystem_" + m_memoryName + ".mem");
    } else {
        // Use SharedMemoryPaths to get the App Group container or fallback directory
        std::string sharedDir = Mach1::SharedMemoryPaths::getMemoryFileDirectory();
        m_tempFile = juce::File(juce::String(sharedDir) + "/M1SpatialSystem_" + m_memoryName + ".mem");
    }

    DBG("[M1MemoryShare] Attempting to create file: " + m_tempFile.getFullPathName());
    DBG("[M1MemoryShare] Total size to allocate: " + juce::String(m_totalSize) + " bytes");

    // Create the file with the required size
    juce::FileOutputStream outputStream(m_tempFile);
    if (!outputStream.openedOk())
    {
        DBG("[M1MemoryShare] Failed to create temp file for shared memory: " + m_tempFile.getFullPathName());
        DBG("[M1MemoryShare] Shared directory: " + juce::String(Mach1::SharedMemoryPaths::getMemoryFileDirectory()));
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
    // Use SharedMemoryPaths to get the App Group container or fallback directory
    std::string sharedDir = Mach1::SharedMemoryPaths::getMemoryFileDirectory();
    if (sharedDir.empty()) {
        // Fallback to temp directory if no shared path available
        sharedDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString();
    }

    m_tempFile = juce::File(juce::String(sharedDir) + "/M1SpatialSystem_" + m_memoryName + ".mem");

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

    // Set up queued buffers pointer (after header)
    m_queuedBuffers = reinterpret_cast<QueuedBuffer*>(
        static_cast<uint8_t*>(m_mappedFile->getData()) + sizeof(SharedMemoryHeader));
    m_queuedBuffersSize = m_maxQueueSize * sizeof(QueuedBuffer);

    // Set up data buffer pointer (after header and queued buffers)
    m_dataBuffer = static_cast<uint8_t*>(m_mappedFile->getData()) +
                   sizeof(SharedMemoryHeader) + m_queuedBuffersSize;
    m_dataBufferSize = m_totalSize - sizeof(SharedMemoryHeader) - m_queuedBuffersSize;

    // Initialize header if we're in create mode
    if (m_createMode)
    {
        *m_header = SharedMemoryHeader(); // Initialize with defaults
        m_header->bufferSize = static_cast<uint32_t>(m_dataBufferSize);
        m_header->maxQueueSize = m_maxQueueSize;
        strncpy(m_header->name, m_memoryName.toUTF8(), sizeof(m_header->name) - 1);

        // Initialize queued buffers array
        for (uint32_t i = 0; i < m_maxQueueSize; ++i)
        {
            m_queuedBuffers[i] = QueuedBuffer();
        }
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

uint64_t M1MemoryShare::writeAudioBufferWithGenericParameters(const juce::AudioBuffer<float>& audioBuffer,
                                                           const ParameterMap& parameters,
                                                           uint64_t dawTimestamp,
                                                           double playheadPositionInSeconds,
                                                           bool isPlaying,
                                                           bool requiresAcknowledgment,
                                                           uint32_t updateSource)
{
    if (!isValid())
    {
        DBG("[M1MemoryShare] writeAudioBufferWithGenericParameters: Not valid");
        return 0;
    }

    // Calculate header size with all parameters
    size_t headerSize = sizeof(GenericAudioBufferHeader);

    // Add size for each parameter type
    for (const auto& pair : parameters.floatParams)
    {
        headerSize += sizeof(GenericParameter) + sizeof(float);
    }
    for (const auto& pair : parameters.intParams)
    {
        headerSize += sizeof(GenericParameter) + sizeof(int32_t);
    }
    for (const auto& pair : parameters.boolParams)
    {
        headerSize += sizeof(GenericParameter) + sizeof(bool);
    }
    for (const auto& pair : parameters.stringParams)
    {
        headerSize += sizeof(GenericParameter) + pair.second.length() + 1; // +1 for null terminator
    }
    for (const auto& pair : parameters.doubleParams)
    {
        headerSize += sizeof(GenericParameter) + sizeof(double);
    }
    for (const auto& pair : parameters.uint32Params)
    {
        headerSize += sizeof(GenericParameter) + sizeof(uint32_t);
    }
    for (const auto& pair : parameters.uint64Params)
    {
        headerSize += sizeof(GenericParameter) + sizeof(uint64_t);
    }

    // Calculate audio data size
    size_t audioDataSize = audioBuffer.getNumSamples() * audioBuffer.getNumChannels() * sizeof(float);
    size_t totalSize = headerSize + audioDataSize;

    if (totalSize > m_dataBufferSize)
    {
        DBG("[M1MemoryShare] Generic buffer too large: " + juce::String(totalSize) + " > " + juce::String(m_dataBufferSize));
        return 0;
    }

    // Generate buffer ID and sequence number
    uint64_t bufferId = getNextBufferId();
    uint32_t sequenceNumber = getNextSequenceNumber();
    uint64_t timestamp = getCurrentTimestamp();

    // If requiresAcknowledgment is true, check if we have space in queue
    if (requiresAcknowledgment)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Clean up acknowledged buffers first
        cleanupAcknowledgedBuffers();

        // Check if queue is full
        if (m_header->queueSize >= m_maxQueueSize)
        {
            DBG("[M1MemoryShare] Queue full, cannot add buffer");
            return 0;
        }
    }

    // Write header
    uint8_t* writePtr = m_dataBuffer;
    GenericAudioBufferHeader* header = reinterpret_cast<GenericAudioBufferHeader*>(writePtr);

    header->version = 1;
    header->channels = audioBuffer.getNumChannels();
    header->samples = audioBuffer.getNumSamples();
    header->dawTimestamp = dawTimestamp;
    header->playheadPositionInSeconds =   playheadPositionInSeconds;
    header->isPlaying = isPlaying ? 1 : 0;
    header->updateSource = updateSource;
    header->isUpdatingFromExternal = 0;
    header->headerSize = static_cast<uint32_t>(headerSize);

    // Add buffer acknowledgment fields
    header->bufferId = bufferId;
    header->sequenceNumber = sequenceNumber;
    header->bufferTimestamp = timestamp;
    header->requiresAcknowledgment = requiresAcknowledgment ? 1 : 0;
    header->consumerCount = m_header->consumerCount;
    header->acknowledgedCount = 0;

    // Count total parameters
    header->parameterCount = parameters.floatParams.size() + parameters.intParams.size() +
                           parameters.boolParams.size() + parameters.stringParams.size() +
                           parameters.doubleParams.size() + parameters.uint32Params.size() +
                           parameters.uint64Params.size();

    writePtr += sizeof(GenericAudioBufferHeader);

    // Write float parameters
    for (const auto& pair : parameters.floatParams)
    {
        GenericParameter* param = reinterpret_cast<GenericParameter*>(writePtr);
        param->parameterID = pair.first;
        param->parameterType = ParameterType::FLOAT;
        param->dataSize = sizeof(float);
        writePtr += sizeof(GenericParameter);

        *reinterpret_cast<float*>(writePtr) = pair.second;
        writePtr += sizeof(float);
    }

    // Write int parameters
    for (const auto& pair : parameters.intParams)
    {
        GenericParameter* param = reinterpret_cast<GenericParameter*>(writePtr);
        param->parameterID = pair.first;
        param->parameterType = ParameterType::INT;
        param->dataSize = sizeof(int32_t);
        writePtr += sizeof(GenericParameter);

        *reinterpret_cast<int32_t*>(writePtr) = pair.second;
        writePtr += sizeof(int32_t);
    }

    // Write bool parameters
    for (const auto& pair : parameters.boolParams)
    {
        GenericParameter* param = reinterpret_cast<GenericParameter*>(writePtr);
        param->parameterID = pair.first;
        param->parameterType = ParameterType::BOOL;
        param->dataSize = sizeof(bool);
        writePtr += sizeof(GenericParameter);

        *reinterpret_cast<bool*>(writePtr) = pair.second;
        writePtr += sizeof(bool);
    }

    // Write string parameters
    for (const auto& pair : parameters.stringParams)
    {
        GenericParameter* param = reinterpret_cast<GenericParameter*>(writePtr);
        param->parameterID = pair.first;
        param->parameterType = ParameterType::STRING;
        param->dataSize = pair.second.length() + 1;
        writePtr += sizeof(GenericParameter);

        strcpy(reinterpret_cast<char*>(writePtr), pair.second.c_str());
        writePtr += pair.second.length() + 1;
    }

    // Write double parameters
    for (const auto& pair : parameters.doubleParams)
    {
        GenericParameter* param = reinterpret_cast<GenericParameter*>(writePtr);
        param->parameterID = pair.first;
        param->parameterType = ParameterType::DOUBLE;
        param->dataSize = sizeof(double);
        writePtr += sizeof(GenericParameter);

        *reinterpret_cast<double*>(writePtr) = pair.second;
        writePtr += sizeof(double);
    }

    // Write uint32 parameters
    for (const auto& pair : parameters.uint32Params)
    {
        GenericParameter* param = reinterpret_cast<GenericParameter*>(writePtr);
        param->parameterID = pair.first;
        param->parameterType = ParameterType::UINT32;
        param->dataSize = sizeof(uint32_t);
        writePtr += sizeof(GenericParameter);

        *reinterpret_cast<uint32_t*>(writePtr) = pair.second;
        writePtr += sizeof(uint32_t);
    }

    // Write uint64 parameters
    for (const auto& pair : parameters.uint64Params)
    {
        GenericParameter* param = reinterpret_cast<GenericParameter*>(writePtr);
        param->parameterID = pair.first;
        param->parameterType = ParameterType::UINT64;
        param->dataSize = sizeof(uint64_t);
        writePtr += sizeof(GenericParameter);

        *reinterpret_cast<uint64_t*>(writePtr) = pair.second;
        writePtr += sizeof(uint64_t);
    }

    // Write audio data
    float* audioDataPtr = reinterpret_cast<float*>(writePtr);
    for (int sample = 0; sample < audioBuffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < audioBuffer.getNumChannels(); ++channel)
        {
            audioDataPtr[sample * audioBuffer.getNumChannels() + channel] = audioBuffer.getSample(channel, sample);
        }
    }

    // Update main header
    m_header->dataSize = static_cast<uint32_t>(totalSize);
    m_header->hasData = true;

    // Add to queue if acknowledgment is required
    if (requiresAcknowledgment)
    {
        addToQueue(bufferId, sequenceNumber, timestamp, totalSize, 0, true);
    }

    // PERFORMANCE FIX: Remove expensive sync() call and use async file modification time updates
    // Memory-mapped files are automatically visible to other processes without sync()
    // Schedule modification time update asynchronously to avoid blocking audio thread
    static std::atomic<int> writeCounter{0};
    if (++writeCounter % 50 == 0) { // Update less frequently (every 50 buffers instead of 10)
        scheduleAsyncFileModTimeUpdate();
    }

    ++m_writeCount;
    DBG("[M1MemoryShare] Wrote generic buffer with " + juce::String(header->parameterCount) + " parameters, bufferId=" + juce::String(bufferId));
    return bufferId;
}

bool M1MemoryShare::readGenericParameters(ParameterMap& parameters,
                                        uint64_t& dawTimestamp,
                                        double& playheadPositionInSeconds,
                                        bool& isPlaying,
                                        uint32_t& updateSource)
{
    if (!isValid() || !m_header->hasData)
    {
        return false;
    }

    if (m_header->dataSize < sizeof(GenericAudioBufferHeader))
    {
        DBG("[M1MemoryShare] Data too small for generic header");
        return false;
    }

    const uint8_t* readPtr = m_dataBuffer;
    const GenericAudioBufferHeader* header = reinterpret_cast<const GenericAudioBufferHeader*>(readPtr);

    // Extract basic info
    dawTimestamp = header->dawTimestamp;
    playheadPositionInSeconds = header->playheadPositionInSeconds;
    isPlaying = header->isPlaying != 0;
    updateSource = header->updateSource;

    readPtr += sizeof(GenericAudioBufferHeader);

    // Clear previous parameters
    parameters.clear();

    // Read all parameters
    for (uint32_t i = 0; i < header->parameterCount; ++i)
    {
        const GenericParameter* param = reinterpret_cast<const GenericParameter*>(readPtr);
        readPtr += sizeof(GenericParameter);

        switch (param->parameterType)
        {
            case ParameterType::FLOAT:
                parameters.floatParams[param->parameterID] = *reinterpret_cast<const float*>(readPtr);
                break;
            case ParameterType::INT:
                parameters.intParams[param->parameterID] = *reinterpret_cast<const int32_t*>(readPtr);
                break;
            case ParameterType::BOOL:
                parameters.boolParams[param->parameterID] = *reinterpret_cast<const bool*>(readPtr);
                break;
            case ParameterType::STRING:
                parameters.stringParams[param->parameterID] = std::string(reinterpret_cast<const char*>(readPtr));
                break;
            case ParameterType::DOUBLE:
                parameters.doubleParams[param->parameterID] = *reinterpret_cast<const double*>(readPtr);
                break;
            case ParameterType::UINT32:
                parameters.uint32Params[param->parameterID] = *reinterpret_cast<const uint32_t*>(readPtr);
                break;
            case ParameterType::UINT64:
                parameters.uint64Params[param->parameterID] = *reinterpret_cast<const uint64_t*>(readPtr);
                break;
        }

        readPtr += param->dataSize;
    }

    ++m_readCount;
    DBG("[M1MemoryShare] Read " + juce::String(header->parameterCount) + " generic parameters");
    return true;
}

bool M1MemoryShare::readAudioBufferWithGenericParameters(juce::AudioBuffer<float>& audioBuffer,
                                                        ParameterMap& parameters,
                                                        uint64_t& dawTimestamp,
                                                        double& playheadPositionInSeconds,
                                                        bool& isPlaying,
                                                        uint64_t& bufferId,
                                                        uint32_t& updateSource)
{
    if (!isValid() || !m_header->hasData)
    {
        return false;
    }

    if (m_header->dataSize < sizeof(GenericAudioBufferHeader))
    {
        DBG("[M1MemoryShare] Data too small for generic header with audio");
        return false;
    }

    const uint8_t* readPtr = m_dataBuffer;
    const GenericAudioBufferHeader* header = reinterpret_cast<const GenericAudioBufferHeader*>(readPtr);

    // Extract basic info
    dawTimestamp = header->dawTimestamp;
    playheadPositionInSeconds = header->playheadPositionInSeconds;
    isPlaying = header->isPlaying != 0;
    updateSource = header->updateSource;
    bufferId = header->bufferId;

    readPtr += sizeof(GenericAudioBufferHeader);

    // Clear previous parameters
    parameters.clear();

    // Read all parameters
    for (uint32_t i = 0; i < header->parameterCount; ++i)
    {
        const GenericParameter* param = reinterpret_cast<const GenericParameter*>(readPtr);
        readPtr += sizeof(GenericParameter);

        switch (param->parameterType)
        {
            case ParameterType::FLOAT:
                parameters.floatParams[param->parameterID] = *reinterpret_cast<const float*>(readPtr);
                break;
            case ParameterType::INT:
                parameters.intParams[param->parameterID] = *reinterpret_cast<const int32_t*>(readPtr);
                break;
            case ParameterType::BOOL:
                parameters.boolParams[param->parameterID] = *reinterpret_cast<const bool*>(readPtr);
                break;
            case ParameterType::STRING:
                parameters.stringParams[param->parameterID] = std::string(reinterpret_cast<const char*>(readPtr));
                break;
            case ParameterType::DOUBLE:
                parameters.doubleParams[param->parameterID] = *reinterpret_cast<const double*>(readPtr);
                break;
            case ParameterType::UINT32:
                parameters.uint32Params[param->parameterID] = *reinterpret_cast<const uint32_t*>(readPtr);
                break;
            case ParameterType::UINT64:
                parameters.uint64Params[param->parameterID] = *reinterpret_cast<const uint64_t*>(readPtr);
                break;
        }

        readPtr += param->dataSize;
    }

    // Calculate audio data position
    const uint8_t* audioDataPos = m_dataBuffer + header->headerSize;

    // Read audio data
    if (header->channels > 0 && header->samples > 0 && header->channels <= 32 && header->samples <= 65536)
    {
        audioBuffer.setSize(header->channels, header->samples, false, true, true);

        const float* audioData = reinterpret_cast<const float*>(audioDataPos);
        for (int sample = 0; sample < header->samples; ++sample)
        {
            for (int channel = 0; channel < header->channels; ++channel)
            {
                audioBuffer.setSample(channel, sample, audioData[sample * header->channels + channel]);
            }
        }
    }

    ++m_readCount;
    DBG("[M1MemoryShare] Read audio buffer with " + juce::String(header->parameterCount) + " generic parameters, bufferId=" + juce::String(bufferId));
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
        stats.queuedBufferCount = 0;
        stats.acknowledgedBufferCount = 0;
        stats.consumerCount = 0;
        return stats;
    }

    stats.totalSize = m_totalSize;
    stats.usedSize = m_header->dataSize;
    stats.availableSize = m_dataBufferSize - stats.usedSize;
    stats.writeCount = m_writeCount.load();
    stats.readCount = m_readCount.load();
    stats.queuedBufferCount = m_header->queueSize;
    stats.acknowledgedBufferCount = 0;
    stats.consumerCount = m_header->consumerCount;

    // Count acknowledged buffers
    for (uint32_t i = 0; i < m_header->queueSize; ++i)
    {
        if (m_queuedBuffers[i].acknowledgedCount == m_queuedBuffers[i].consumerCount)
        {
            stats.acknowledgedBufferCount++;
        }
    }

    return stats;
}

// Buffer management methods
uint64_t M1MemoryShare::getNextBufferId()
{
    return m_header->nextBufferId++;
}

uint32_t M1MemoryShare::getNextSequenceNumber()
{
    return m_header->nextSequenceNumber++;
}

uint64_t M1MemoryShare::getCurrentTimestamp() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// Queue management methods
bool M1MemoryShare::addToQueue(uint64_t bufferId, uint32_t sequenceNumber, uint64_t timestamp,
                              uint32_t dataSize, uint32_t dataOffset, bool requiresAcknowledgment)
{
    if (m_header->queueSize >= m_maxQueueSize)
    {
        return false;
    }

    // Find empty slot
    uint32_t slotIndex = m_header->queueSize;

    QueuedBuffer& buffer = m_queuedBuffers[slotIndex];
    buffer.bufferId = bufferId;
    buffer.sequenceNumber = sequenceNumber;
    buffer.timestamp = timestamp;
    buffer.dataSize = dataSize;
    buffer.dataOffset = dataOffset;
    buffer.requiresAcknowledgment = requiresAcknowledgment;
    buffer.consumerCount = m_header->consumerCount;
    buffer.acknowledgedCount = 0;

    // Copy consumer IDs
    for (uint32_t i = 0; i < m_header->consumerCount; ++i)
    {
        buffer.consumerIds[i] = m_header->consumerIds[i];
        buffer.acknowledged[i] = false;
    }

    m_header->queueSize++;
    return true;
}

bool M1MemoryShare::removeFromQueue(uint64_t bufferId)
{
    // Find buffer in queue
    for (uint32_t i = 0; i < m_header->queueSize; ++i)
    {
        if (m_queuedBuffers[i].bufferId == bufferId)
        {
            // Move all subsequent buffers down
            for (uint32_t j = i; j < m_header->queueSize - 1; ++j)
            {
                m_queuedBuffers[j] = m_queuedBuffers[j + 1];
            }
            m_header->queueSize--;
            return true;
        }
    }
    return false;
}

M1MemoryShare::QueuedBuffer* M1MemoryShare::findQueuedBuffer(uint64_t bufferId)
{
    for (uint32_t i = 0; i < m_header->queueSize; ++i)
    {
        if (m_queuedBuffers[i].bufferId == bufferId)
        {
            return &m_queuedBuffers[i];
        }
    }
    return nullptr;
}

void M1MemoryShare::cleanupAcknowledgedBuffers()
{
    // Remove fully acknowledged buffers
    for (uint32_t i = 0; i < m_header->queueSize; )
    {
        if (m_queuedBuffers[i].acknowledgedCount >= m_queuedBuffers[i].consumerCount)
        {
            removeFromQueue(m_queuedBuffers[i].bufferId);
        }
        else
        {
            ++i;
        }
    }
}

// Consumer management methods
bool M1MemoryShare::registerConsumer(uint32_t consumerId)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    // Check if consumer is already registered
    if (isConsumerRegistered(consumerId))
    {
        return true;
    }

    // Check if we have space
    if (m_header->consumerCount >= 16)
    {
        return false;
    }

    // Add consumer
    m_header->consumerIds[m_header->consumerCount] = consumerId;
    m_header->consumerCount++;

    return true;
}

bool M1MemoryShare::unregisterConsumer(uint32_t consumerId)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    int index = findConsumerIndex(consumerId);
    if (index == -1)
    {
        return false;
    }

    // Remove consumer by shifting others down
    for (uint32_t i = index; i < m_header->consumerCount - 1; ++i)
    {
        m_header->consumerIds[i] = m_header->consumerIds[i + 1];
    }
    m_header->consumerCount--;

    return true;
}

int M1MemoryShare::findConsumerIndex(uint32_t consumerId) const
{
    for (uint32_t i = 0; i < m_header->consumerCount; ++i)
    {
        if (m_header->consumerIds[i] == consumerId)
        {
            return i;
        }
    }
    return -1;
}

bool M1MemoryShare::isConsumerRegistered(uint32_t consumerId) const
{
    return findConsumerIndex(consumerId) != -1;
}

// Buffer acknowledgment methods
bool M1MemoryShare::acknowledgeBuffer(uint64_t bufferId, uint32_t consumerId)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    QueuedBuffer* buffer = findQueuedBuffer(bufferId);
    if (!buffer)
    {
        return false;
    }

    // Find consumer in buffer's consumer list
    for (uint32_t i = 0; i < buffer->consumerCount; ++i)
    {
        if (buffer->consumerIds[i] == consumerId && !buffer->acknowledged[i])
        {
            buffer->acknowledged[i] = true;
            buffer->acknowledgedCount++;
            return true;
        }
    }

    return false;
}

std::vector<uint64_t> M1MemoryShare::getAvailableBufferIds() const
{
    std::vector<uint64_t> bufferIds;

    for (uint32_t i = 0; i < m_header->queueSize; ++i)
    {
        bufferIds.push_back(m_queuedBuffers[i].bufferId);
    }

    return bufferIds;
}

uint32_t M1MemoryShare::getUnconsumedBufferCount() const
{
    return m_header->queueSize;
}

bool M1MemoryShare::readBufferById(uint64_t bufferId,
                                  juce::AudioBuffer<float>& audioBuffer,
                                  ParameterMap& parameters,
                                  uint64_t& dawTimestamp,
                                  double& playheadPositionInSeconds,
                                  bool& isPlaying,
                                  uint32_t& updateSource)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    QueuedBuffer* queuedBuffer = findQueuedBuffer(bufferId);
    if (!queuedBuffer)
    {
        return false;
    }

    // For now, we only support reading the current buffer in the data buffer
    // In a full implementation, we would need to store multiple buffers
    // and use queuedBuffer->dataOffset to read from the correct position

    // Read the current buffer (assuming it's the one we want)
    uint64_t currentBufferId;
    return readAudioBufferWithGenericParameters(audioBuffer, parameters, dawTimestamp,
                                               playheadPositionInSeconds, isPlaying,
                                               currentBufferId, updateSource) &&
           currentBufferId == bufferId;
}

//==============================================================================
bool M1MemoryShare::deleteSharedMemory(const juce::String& memoryName)
{
    // Use SharedMemoryPaths to get the App Group container or fallback directory
    std::string sharedDir = Mach1::SharedMemoryPaths::getMemoryFileDirectory();
    if (sharedDir.empty()) {
        // Fallback to temp directory if no shared path available
        sharedDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString();
    }

    juce::File memoryFile(juce::String(sharedDir) + "/M1SpatialSystem_" + memoryName + ".mem");

    if (memoryFile.exists())
    {
        return memoryFile.deleteFile();
    }

    return true; // File doesn't exist, consider it deleted
}

// PERFORMANCE FIX: Async file modification time update implementation
void M1MemoryShare::scheduleAsyncFileModTimeUpdate()
{
    // Use JUCE's async message manager to update file modification time on main thread
    // This avoids blocking the audio thread with file I/O operations
    if (m_tempFile.exists())
    {
        juce::MessageManager::callAsync([this]() {
            if (m_tempFile.exists()) {
                m_tempFile.setLastModificationTime(juce::Time::getCurrentTime());
                DBG("[M1MemoryShare] Async file mod time update: " + m_tempFile.getFullPathName());
            }
        });
    }
}
