#include "PannerOSC.h"

void PannerOSC::oscMessageReceived(const juce::OSCMessage& msg)
{
	if (messageReceived != nullptr)
	{
		messageReceived(msg);
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
	 
	// check port
	juce::DatagramSocket socket(false);
	socket.setEnablePortReuse(false);

    std::string settingsFilePath = (juce::File::getCurrentWorkingDirectory().getFullPathName() + "/settings.json").toStdString();
    initFromSettings(settingsFilePath);
    
	// find available port
	for (port = 10001; port < 10200; port++)
	{
		if (socket.bindToPort(port))
		{
			socket.shutdown();
            juce::OSCReceiver::connect(port);
			break;
		}
	}
    juce::OSCReceiver::addListener(this);
}

void PannerOSC::update()
{
	if (!isConnected)
	{
		if (juce::OSCSender::connect("127.0.0.1", serverPort))
		{
            juce::OSCMessage msg("/m1-register-plugin/port");
			msg.addInt32(port);
			isConnected = juce::OSCSender::send(msg);
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

