#include "PannerOSC.h"

bool PannerOSC::init(int helperPort) {
    // check port
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    this->helperPort = helperPort;
    
    // find available port
    for (port = 10001; port < 10200; port++) {
        if (socket.bindToPort(port)) {
            socket.shutdown();
            juce::OSCReceiver::connect(port);
            break; // stops the incrementing on the first available port
        }
    }
    
    if (port > 10000) {
        return true;
    }
}

// finds the server port via the settings json file
bool PannerOSC::initFromSettings(std::string jsonSettingsFilePath) {
    juce::File settingsFile = juce::File(jsonSettingsFilePath);
    if (!settingsFile.exists()) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::NoIcon,
            "Warning",
            "Settings file doesn't exist",
            "",
            nullptr,
            juce::ModalCallbackFunction::create(([&](int result) {
                //juce::JUCEApplicationBase::quit();
            }))
        );
        return false;
    }
    else {
        juce::var mainVar = juce::JSON::parse(juce::File(jsonSettingsFilePath));
        int helperPort = mainVar["helperPort"];

        if (!init(helperPort)) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Warning",
                "Conflict is happening and you need to choose a new port",
                "",
                nullptr,
                juce::ModalCallbackFunction::create(([&](int result) {
                   // juce::JUCEApplicationBase::quit();
                }))
            );
            return false;
        }
    }
    return true;
}

PannerOSC::PannerOSC()
{
	isConnected = false;
    
    // We will assume the folders are properly created during the installation step
    juce::File settingsFile;
    // Using common support files installation location
    juce::File m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

    if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
        // test for any mac OS
        settingsFile = m1SupportDirectory.getChildFile("Application Support").getChildFile("Mach1");
    } else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
        // test for any windows OS
        settingsFile = m1SupportDirectory.getChildFile("Mach1");
    } else {
        settingsFile = m1SupportDirectory.getChildFile("Mach1");
    }
    settingsFile = settingsFile.getChildFile("settings.json");
    DBG("Opening settings file: " + settingsFile.getFullPathName().quoted());
    
    initFromSettings(settingsFile.getFullPathName().toStdString());
    juce::OSCReceiver::addListener(this);
}

void PannerOSC::oscMessageReceived(const juce::OSCMessage& msg)
{
    if (messageReceived != nullptr) {
        messageReceived(msg);
    }
    lastMessageTime = juce::Time::getMillisecondCounter();
}

void PannerOSC::update()
{
	if (!isConnected && helperPort > 0) {
		if (juce::OSCSender::connect("127.0.0.1", helperPort)) {
            juce::OSCMessage msg = juce::OSCMessage(juce::OSCAddressPattern("/m1-register-plugin"));
			msg.addInt32(port);
			isConnected = juce::OSCSender::send(msg);
            DBG("[OSC] Registered: "+std::to_string(port));
		}
	}
    
    if (isConnected) {
        juce::uint32 currentTime = juce::Time::getMillisecondCounter();
        if ((currentTime - lastMessageTime) > 10000) { // 10000 milliseconds = 10 seconds
            isConnected = false;
        }
    }
}

void PannerOSC::AddListener(std::function<void(juce::OSCMessage msg)> messageReceived)
{
	this->messageReceived = messageReceived;
}

PannerOSC::~PannerOSC()
{
    if (port > 0) {
        // send a "remove panner" message to helper
        juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/panner-settings"));
        m.addInt32(port); // used for id
        m.addInt32(-1);   // sending a -1 to indicate a disconnect command via the state
        juce::OSCSender::send(m);
    }
    
    juce::OSCSender::disconnect();
    juce::OSCReceiver::disconnect();
}

bool PannerOSC::Send(const juce::OSCMessage& msg)
{
	return (isConnected && juce::OSCSender::send(msg));
}

bool PannerOSC::IsConnected()
{
	return isConnected;
}

bool PannerOSC::sendPannerSettings(int state)
{
    if (isConnected && port > 0) {
        // Each call will transmit an OSC message with the relevant current panner settings
        juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/panner-settings"));
        m.addInt32(port);        // used for id
        m.addInt32(state);       // used for panner interactive state
        isConnected = juce::OSCSender::send(m); // check to update isConnected for error catching
        return true;
    }
    return false;
}

bool PannerOSC::sendPannerSettings(int state, std::string displayName, juce::OSCColour colour, int input_mode, float azimuth, float elevation, float diverge, float gain, int panner_mode, bool st_auto_orbit, float st_azimuth, float st_spread)
{
	if (isConnected && port > 0) {
		// Each call will transmit an OSC message with the relevant current panner settings
		juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/panner-settings"));
		m.addInt32(port);        // [msg[0]]: used for id
        m.addInt32(state);       // [msg[1]]: used for panner interactive state
        m.addString(displayName);// [msg[2]]: string for track name (when available)
        m.addColour(colour);     // [msg[3]]: hex for track color (when available)
		m.addInt32(input_mode);  // [msg[4]]: int of enum `Mach1EncodeInputModeType`
		m.addFloat32(azimuth);   // [msg[5]]: expected degrees -180->180
		m.addFloat32(elevation); // [msg[6]]: expected degrees -90->90
		m.addFloat32(diverge);   // [msg[7]]: expected normalized -100->100
		m.addFloat32(gain);      // [msg[8]]: expected as dB value -90->24
		m.addInt32(panner_mode); // [msg[9]]: int of enum `Mach1EncodePannerModeType`
		if (input_mode == 1) {
            // send stereo parameters
            m.addInt32(st_auto_orbit); // [msg[10]]: bool
            m.addFloat32(st_azimuth);  // [msg[11]]: expected degrees -180->180
            m.addFloat32(st_spread);   // [msg[12]]: expected normalized -100->100
        }
		isConnected = juce::OSCSender::send(m); // check to update isConnected for error catching
		return true;
	}
	return false;
}
