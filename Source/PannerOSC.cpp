#include "PannerOSC.h"

#include "PluginProcessor.h"

PannerOSC::PannerOSC(M1PannerAudioProcessor* processor_)
{
    processor = processor_;
    is_connected = false;

    // We will assume the folders are properly created during the installation step
    juce::File settingsFile;
    // Using common support files installation location
    juce::File m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

    if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0)
    {
        // test for any mac OS
        settingsFile = m1SupportDirectory.getChildFile("Application Support").getChildFile("Mach1");
    }
    else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0)
    {
        // test for any windows OS
        settingsFile = m1SupportDirectory.getChildFile("Mach1");
    }
    else
    {
        settingsFile = m1SupportDirectory.getChildFile("Mach1");
    }
    settingsFile = settingsFile.getChildFile("settings.json");
    DBG("Opening settings file: " + settingsFile.getFullPathName().quoted());

    initFromSettings(settingsFile.getFullPathName().toStdString());
    juce::OSCReceiver::addListener(this);
}

PannerOSC::~PannerOSC()
{
    if (is_connected && port > 0)
    {
        // send a "remove panner" message to helper
        juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/panner-settings"));
        m.addInt32(port); // used for id
        m.addInt32(-1); // sending a -1 to indicate a disconnect command via the state
        juce::OSCSender::send(m);
    }

    juce::OSCSender::disconnect();
    juce::OSCReceiver::disconnect();
}

bool PannerOSC::init(int helperPort_)
{
    helperPort = helperPort_;

    // Try to find an available port for the receiver
    bool receiverConnected = false;
    int attempts = 0;
    const int maxAttempts = 100;

    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);

    while (!receiverConnected && attempts < maxAttempts) {
        port = 10000 + juce::Random::getSystemRandom().nextInt(1000);
        if (socket.bindToPort(port)) {
            socket.shutdown(); // shutdown port to not block the juce::OSCReceiver::connect return
            receiverConnected = juce::OSCReceiver::connect(port);
        }
        attempts++;
    }

    if (!receiverConnected) {
        // Add alert for failed OSC receiver connection
        if (processor) {
            Mach1::AlertData alert;
            alert.title = "Connection Error";
            alert.message = "Failed to connect OSC receiver after multiple attempts. Network features may not work correctly.";
            alert.buttonText = "OK";
            processor->postAlert(alert);
        }
        return false;
    } else {
        // Try to connect to the helper application
        if (helperPort > 0) {
            if (juce::OSCSender::connect("127.0.0.1", helperPort)) {
                juce::OSCMessage msg = juce::OSCMessage(juce::OSCAddressPattern("/m1-register-plugin"));
                msg.addInt32(port);
                is_connected = juce::OSCSender::send(msg);
                DBG("[OSC] Registered: " + std::to_string(port));
            } else {
                // Add alert for failed helper connection
                if (processor) {
                    Mach1::AlertData alert;
                    alert.title = "Connection Warning";
                    alert.message = "Could not connect to m1-system-helper. Some features may be limited.";
                    alert.buttonText = "OK";
                    processor->postAlert(alert);
                }
            }
        }
        juce::OSCReceiver::addListener(this);
        return true;
    }
}

// finds the server port via the settings json file
bool PannerOSC::initFromSettings(const std::string& jsonSettingsFilePath)
{
    juce::File settingsFile(jsonSettingsFilePath);

    if (!settingsFile.exists())
    {
        if (processor)
        {
            Mach1::AlertData data { "Warning", "The settings.json file doesn't exist in Mach1's Application Support directory!\nPlease reinstall the Mach1 Spatial System.", "OK" };
            processor->postAlert(data);
        }
        return false;
    }
    else
    {
        juce::var mainVar = juce::JSON::parse(juce::File(jsonSettingsFilePath));
        int helperPort = mainVar["helperPort"];

        if (!init(helperPort))
        {
            if (processor)
            {
                Mach1::AlertData data { "Warning", "Could not connect to the m1-system-helper!\nPlease reinstall Mach1 Spatial System", "OK" };
                processor->postAlert(data);
            }
        }
    }
    return true;
}

void PannerOSC::oscMessageReceived(const juce::OSCMessage& msg)
{
    if (messageReceived != nullptr)
    {
        if (msg.getAddressPattern() == "/m1-ping")
        {
            try
            {
                // Create a temporary sender for the ping response to avoid disturbing main connection
                juce::OSCSender tempSender;
                if (tempSender.connect("127.0.0.1", helperPort))
                {
                    juce::OSCMessage response = juce::OSCMessage(juce::OSCAddressPattern("/m1-status-plugin"));
                    response.addInt32(port);
                    is_connected = tempSender.send(response);
                    //DBG("[OSC] Ping responded: " + std::to_string(port));
                }
            }
            catch (...)
            {
                DBG("[OSC] Failed to respond to ping");
                is_connected = false;
            }
        }
        else if (msg.getAddressPattern() == "/m1-streaming-ack")
        {
            // Helper acknowledged streaming registration
            streamingAcknowledged = true;
            DBG("[OSC] Streaming registration acknowledged");
        }
        else if (msg.getAddressPattern() == "/m1-streaming-nack")
        {
            // Helper rejected streaming registration
            streamingAcknowledged = false;
            if (msg.size() > 0 && msg[0].isString()) {
                DBG("[OSC] Streaming registration rejected: " + msg[0].getString().toStdString());
            } else {
                DBG("[OSC] Streaming registration rejected");
            }
        }
        else
        {
            messageReceived(msg);
        }
    }
    lastMessageTime = juce::Time::getMillisecondCounter();
}

void PannerOSC::update()
{
    if (!is_connected && helperPort > 0)
    {
        if (juce::OSCSender::connect("127.0.0.1", helperPort))
        {
            juce::OSCMessage msg = juce::OSCMessage(juce::OSCAddressPattern("/m1-register-plugin"));
            msg.addInt32(port);
            is_connected = juce::OSCSender::send(msg);
            DBG("[OSC] Registered: " + std::to_string(port));
        }
    }

    if (is_connected)
    {
        juce::uint32 currentTime = juce::Time::getMillisecondCounter();
        if ((currentTime - lastMessageTime) > 10000)
        { // 10000 milliseconds = 10 seconds
            is_connected = false;
        }
    }
}

void PannerOSC::AddListener(std::function<void(juce::OSCMessage msg)> messageReceived)
{
    this->messageReceived = messageReceived;
}

bool PannerOSC::Send(const juce::OSCMessage& msg)
{
    return (is_connected && juce::OSCSender::send(msg));
}

bool PannerOSC::isConnected()
{
    return is_connected;
}

bool PannerOSC::sendRequestForCurrentChannelConfig()
{
    if (!is_connected) {
        // Try to reconnect if needed
        try {
            if (!juce::OSCSender::connect("127.0.0.1", helperPort)) {
                if (processor) {
                    Mach1::AlertData alert;
                    alert.title = "Connection Error";
                    alert.message = "Failed to connect to m1-system-helper when requesting current channel configuration.";
                    alert.buttonText = "OK";
                    processor->postAlert(alert);
                }
                return false;
            }
        } catch (...) {
            if (processor) {
                Mach1::AlertData alert;
                alert.title = "Connection Error";
                alert.message = "Exception occurred when connecting to m1-system-helper.";
                alert.buttonText = "OK";
                processor->postAlert(alert);
            }
            return false;
        }
    }

    // Build message to request current channel config
    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/request-current-channel-config"));
    try {
        m.addInt32(port); // Include our port for identification

        if (!juce::OSCSender::send(m)) {
            is_connected = false;
            if (processor) {
                Mach1::AlertData alert;
                alert.title = "Request Error";
                alert.message = "Failed to send request for current channel configuration to m1-system-helper.";
                alert.buttonText = "OK";
                processor->postAlert(alert);
            }
            return false;
        }
        return true;
    } catch (...) {
        is_connected = false;
        if (processor) {
            Mach1::AlertData alert;
            alert.title = "Request Error";
            alert.message = "Exception occurred when requesting current channel configuration.";
            alert.buttonText = "OK";
            processor->postAlert(alert);
        }
        return false;
    }
}

bool PannerOSC::sendPannerSettings(int state)
{
    if (!isConnected() || port <= 0)
        return false;

    // Try to reconnect if needed - this will return false if connection fails
    try {
        if (!juce::OSCSender::connect("127.0.0.1", helperPort))
        {
            is_connected = false;
            return false;
        }
    }
    catch (...) {
        is_connected = false;
        return false;
    }

    // Build message
    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/panner-settings"));
    try {
        m.addInt32(port); // used for id
        m.addInt32(state); // used for panner interactive state
    }
    catch (...) {
        is_connected = false;
        return false;
    }

    // Add extra protection around the send operation
    try {
        if (!juce::OSCSender::send(m)) {
            is_connected = false;
            return false;
        }
        return true;
    }
    catch (...) {
        is_connected = false;
        return false;
    }
}

bool PannerOSC::sendPannerSettings(int state, std::string displayName, juce::OSCColour colour, int input_mode, float azimuth, float elevation, float diverge, float gain, int panner_mode, bool gain_comp_active, bool st_auto_orbit, float st_azimuth, float st_spread)
{
    if (!isConnected() || port <= 0)
        return false;

    // Try to reconnect if needed - this will return false if connection fails
    try {
        if (!juce::OSCSender::connect("127.0.0.1", helperPort))
        {
            is_connected = false;
            return false;
        }
    }
    catch (...) {
        is_connected = false;
        return false;
    }

    // Build message
    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/panner-settings"));
    try {
        m.addInt32(port); // [msg[0]]: used for id
        m.addInt32(state); // [msg[1]]: used for panner interactive state
        m.addString(displayName); // [msg[2]]: string for track name (when available)
        m.addColour(colour); // [msg[3]]: hex for track color (when available)
        m.addInt32(input_mode); // [msg[4]]: int of enum `Mach1EncodeInputModeType`
        m.addFloat32(azimuth); // [msg[5]]: expected degrees -180->180
        m.addFloat32(elevation); // [msg[6]]: expected degrees -90->90
        m.addFloat32(diverge); // [msg[7]]: expected normalized -100->100
        m.addFloat32(gain); // [msg[8]]: expected as dB value -90->24
        m.addInt32(panner_mode); // [msg[9]]: int of enum `Mach1EncodePannerModeType`
        m.addInt32(gain_comp_active); // [msg[13]: bool
        if (input_mode == 1)
        {
            // send stereo parameters
            m.addInt32(st_auto_orbit); // [msg[10]]: bool
            m.addFloat32(st_azimuth); // [msg[11]]: expected degrees -180->180
            m.addFloat32(st_spread); // [msg[12]]: expected normalized -100->100
        }
    }
    catch (...) {
        is_connected = false;
        return false;
    }

    // Add extra protection around the send operation
    try {
        if (!juce::OSCSender::send(m)) {
            is_connected = false;
            return false;
        }
        return true;
    }
    catch (...) {
        is_connected = false;
        return false;
    }
}

//==============================================================================
// Streaming Mode OSC Methods
//==============================================================================

bool PannerOSC::registerStreamingPanner(const juce::String& sessionId,
                                        const juce::String& pannerUuid,
                                        const juce::String& mmapPath,
                                        int numChannels,
                                        int sampleRate,
                                        int maxBlockSize)
{
    if (helperPort <= 0)
        return false;

    // Try to connect to helper
    try {
        if (!juce::OSCSender::connect("127.0.0.1", helperPort))
        {
            DBG("[PannerOSC] Failed to connect to helper for streaming registration");
            return false;
        }
    }
    catch (...) {
        DBG("[PannerOSC] Exception connecting to helper for streaming registration");
        return false;
    }

    // Build streaming registration message
    // /m1-streaming-register sessionId pannerUuid mmapPath numChannels sampleRate maxBlockSize port
    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/m1-streaming-register"));
    try {
        m.addString(sessionId);           // [0]: Host PID as string (groups panners)
        m.addString(pannerUuid);          // [1]: Unique panner identifier
        m.addString(mmapPath);            // [2]: Path to memory-mapped file
        m.addInt32(numChannels);          // [3]: Number of M1 output channels
        m.addInt32(sampleRate);           // [4]: Sample rate
        m.addInt32(maxBlockSize);         // [5]: Maximum block size
        m.addInt32(port);                 // [6]: OSC reply port for this panner
    }
    catch (...) {
        DBG("[PannerOSC] Exception building streaming registration message");
        return false;
    }

    // Send registration
    try {
        if (!juce::OSCSender::send(m)) {
            DBG("[PannerOSC] Failed to send streaming registration");
            return false;
        }
        
        DBG("[PannerOSC] Sent streaming registration: UUID=" + pannerUuid + 
            " session=" + sessionId + " channels=" + juce::String(numChannels));
        return true;
    }
    catch (...) {
        DBG("[PannerOSC] Exception sending streaming registration");
        return false;
    }
}

bool PannerOSC::unregisterStreamingPanner(const juce::String& pannerUuid)
{
    if (helperPort <= 0)
        return false;

    // Try to connect to helper
    try {
        if (!juce::OSCSender::connect("127.0.0.1", helperPort))
        {
            return false;
        }
    }
    catch (...) {
        return false;
    }

    // Build unregistration message
    // /m1-streaming-unregister pannerUuid
    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/m1-streaming-unregister"));
    try {
        m.addString(pannerUuid);          // [0]: Unique panner identifier
        m.addInt32(port);                 // [1]: OSC port for identification
    }
    catch (...) {
        return false;
    }

    // Send unregistration
    try {
        if (!juce::OSCSender::send(m)) {
            return false;
        }
        
        streamingAcknowledged = false;
        DBG("[PannerOSC] Sent streaming unregistration: UUID=" + pannerUuid);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool PannerOSC::sendStreamingHeartbeat(const juce::String& pannerUuid)
{
    if (!is_connected || helperPort <= 0)
        return false;

    // Build heartbeat message
    // /m1-streaming-heartbeat pannerUuid port
    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/m1-streaming-heartbeat"));
    try {
        m.addString(pannerUuid);          // [0]: Unique panner identifier  
        m.addInt32(port);                 // [1]: OSC port for identification
    }
    catch (...) {
        return false;
    }

    // Send heartbeat
    try {
        return juce::OSCSender::send(m);
    }
    catch (...) {
        return false;
    }
}
