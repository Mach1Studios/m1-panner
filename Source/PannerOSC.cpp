#include "PannerOSC.h"

bool PannerOSC::init(int serverPort) {
    // check port
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    this->serverPort = serverPort;
    
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
                juce::JUCEApplicationBase::quit();
                }))
        );
        return false;
    }
    else {
        juce::var mainVar = juce::JSON::parse(juce::File(jsonSettingsFilePath));
        int serverPort = mainVar["serverPort"];

        if (!init(serverPort)) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Warning",
                "Conflict is happening and you need to choose a new port",
                "",
                nullptr,
                juce::ModalCallbackFunction::create(([&](int result) {
                    juce::JUCEApplicationBase::quit();
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
	if (!isConnected) {
		if (juce::OSCSender::connect("127.0.0.1", serverPort)) {
            juce::OSCMessage msg("/m1-register-plugin");
			msg.addInt32(port);
			isConnected = juce::OSCSender::send(msg);
            DBG("[OSC] Registered: "+std::to_string(port));
		}
	}
    
    if (isConnected) {
        uint32 currentTime = juce::Time::getMillisecondCounter();
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

