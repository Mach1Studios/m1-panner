#pragma once

#include <JuceHeader.h>
#include <string>
#include <set>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

namespace Mach1 {

/**
 * @brief Cross-platform utility class for managing the system helper service on-demand
 *
 * Supports:
 * - macOS: launchd with socket activation
 * - Windows: Windows Service Control Manager with named pipes
 * - Linux: systemd with socket activation
 *
 * Usage is identical across all platforms:
 * ```cpp
 * auto& manager = M1SystemHelperManager::getInstance();
 * manager.requestHelperService("M1SpatialSystem_M1Panner_PID123_...");
 * // ... do work ...
 * manager.releaseHelperService("M1SpatialSystem_M1Panner_PID123_...");
 * ```
 */
class M1SystemHelperManager
{
public:
    static M1SystemHelperManager& getInstance()
    {
        static M1SystemHelperManager instance;
        return instance;
    }

    /**
     * @brief Request the helper service to start (if not already running)
     * @param instanceId Unique identifier for this panner instance
     * @return true if service is now running, false on error
     */
    bool requestHelperService(const std::string& instanceId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Add this instance to the active list
        auto insertResult = m_activeInstances.insert(instanceId);
        bool wasNewInstance = insertResult.second;

        DBG("[M1SystemHelperManager] " + juce::String(instanceId) + " requested service. "
            "Active instances: " + juce::String(static_cast<int>(m_activeInstances.size())) +
            (wasNewInstance ? " (new instance)" : " (existing instance)"));

        // If service is already running, we're done
        if (isHelperServiceRunning()) {
            DBG("[M1SystemHelperManager] Service already running");
            return true;
        }

        DBG("[M1SystemHelperManager] Starting helper service...");

        // Platform-specific service startup
        if (!startHelperService()) {
            DBG("[M1SystemHelperManager] Failed to start service");
            m_launchFailed = true;
            return false;
        }

        // Give service time to start
        juce::Thread::sleep(1000);

        return isHelperServiceRunning();
    }

    /**
     * @brief Release the helper service (may stop if no other instances need it)
     * @param instanceId Unique identifier for this panner instance
     */
    void releaseHelperService(const std::string& instanceId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_activeInstances.erase(instanceId);

        DBG("[M1SystemHelperManager] " + juce::String(instanceId) + " released helper service. "
            "Active instances: " + juce::String(static_cast<int>(m_activeInstances.size())));

        if (m_activeInstances.empty()) {
            DBG("[M1SystemHelperManager] No more active instances. Service will auto-exit.");
        }
    }

    /**
     * @brief Check if the helper service is currently running
     * @return true if running, false otherwise
     */
    bool isHelperServiceRunning() const
    {
        // Check if we can connect to the helper's OSC port
        juce::DatagramSocket socket(false);
        socket.setEnablePortReuse(false);
        
        // Try to bind to helper port - if it fails, helper is running
        bool bound = socket.bindToPort(getHelperPort());
        socket.shutdown();
        
        return !bound;  // If we couldn't bind, port is in use (helper running)
    }

    /**
     * @brief Check if helper launch has previously failed
     * @return true if launch failed (user should start manually)
     */
    bool hasLaunchFailed() const { return m_launchFailed; }

    /**
     * @brief Get the number of instances currently using the service
     */
    int getActiveInstanceCount() const 
    { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<int>(m_activeInstances.size()); 
    }

    /**
     * @brief Check if any M1-Panner instances are currently active
     */
    bool hasActiveM1PannerInstances() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& instance : m_activeInstances) {
            if (instance.find("M1Panner") != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    M1SystemHelperManager() = default;
    ~M1SystemHelperManager() = default;

    // Disable copy/move
    M1SystemHelperManager(const M1SystemHelperManager&) = delete;
    M1SystemHelperManager& operator=(const M1SystemHelperManager&) = delete;

    /**
     * @brief Get the helper OSC port from settings
     */
    int getHelperPort() const
    {
        // Try to read from settings.json
        juce::File settingsFile;
        
        #if JUCE_MAC
            settingsFile = juce::File("/Library/Application Support/Mach1/settings.json");
        #elif JUCE_WINDOWS
            settingsFile = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                .getChildFile("Mach1/settings.json");
        #else
            settingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("Mach1/settings.json");
        #endif
        
        if (settingsFile.exists()) {
            auto json = juce::JSON::parse(settingsFile);
            if (json.hasProperty("helperPort")) {
                return static_cast<int>(json["helperPort"]);
            }
        }
        
        return 9001;  // Default helper port
    }

    /**
     * @brief Start the helper service
     * @return true if service started successfully
     */
    bool startHelperService()
    {
        #if JUCE_MAC
            return startHelperServiceMacOS();
        #elif JUCE_WINDOWS
            return startHelperServiceWindows();
        #else
            return startHelperServiceLinux();
        #endif
    }

#if JUCE_MAC
    bool startHelperServiceMacOS()
    {
        // Try launchctl first (if installed as launch agent)
        std::string loadCmd = "launchctl load /Library/LaunchAgents/com.mach1.spatial.helper.plist 2>/dev/null";
        int loadResult = std::system(loadCmd.c_str());
        
        std::string startCmd = "launchctl start com.mach1.spatial.helper 2>/dev/null";
        int startResult = std::system(startCmd.c_str());
        
        if (startResult == 0 || loadResult == 0) {
            DBG("[M1SystemHelperManager] Launched via launchctl");
            return true;
        }
        
        // Fall back to direct app launch
        juce::File helperApp("/Applications/Mach1 Spatial System/m1-system-helper.app");
        if (!helperApp.exists()) {
            helperApp = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                .getChildFile("Application Support/Mach1/m1-system-helper.app");
        }
        
        if (helperApp.exists()) {
            std::string openCmd = "open \"" + helperApp.getFullPathName().toStdString() + "\" &";
            int result = std::system(openCmd.c_str());
            if (result == 0) {
                DBG("[M1SystemHelperManager] Launched via open command: " + helperApp.getFullPathName());
                return true;
            }
        }
        
        DBG("[M1SystemHelperManager] Could not find or launch helper on macOS");
        return false;
    }
#endif

#if JUCE_WINDOWS
    bool startHelperServiceWindows()
    {
        // Try Windows Service first
        SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (scManager) {
            SC_HANDLE service = OpenService(scManager, L"M1-System-Helper", SERVICE_START);
            if (service) {
                BOOL result = StartService(service, 0, NULL);
                CloseServiceHandle(service);
                CloseServiceHandle(scManager);
                if (result || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) {
                    DBG("[M1SystemHelperManager] Started Windows service");
                    return true;
                }
            }
            CloseServiceHandle(scManager);
        }
        
        // Fall back to direct exe launch
        juce::File helperExe("C:/Program Files/Mach1 Spatial System/m1-system-helper.exe");
        if (!helperExe.exists()) {
            helperExe = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                .getChildFile("Mach1/m1-system-helper.exe");
        }
        
        if (helperExe.exists()) {
            juce::URL("file://" + helperExe.getFullPathName()).launchInDefaultBrowser();
            DBG("[M1SystemHelperManager] Launched via URL: " + helperExe.getFullPathName());
            return true;
        }
        
        DBG("[M1SystemHelperManager] Could not find or launch helper on Windows");
        return false;
    }
#endif

#if !JUCE_MAC && !JUCE_WINDOWS
    bool startHelperServiceLinux()
    {
        // Try systemctl first
        std::string startCmd = "systemctl --user start m1-system-helper 2>/dev/null";
        int result = std::system(startCmd.c_str());
        
        if (result == 0) {
            DBG("[M1SystemHelperManager] Started via systemctl");
            return true;
        }
        
        // Fall back to direct launch
        juce::File helperBin("/usr/local/bin/m1-system-helper");
        if (!helperBin.exists()) {
            helperBin = juce::File("/opt/mach1/m1-system-helper");
        }
        
        if (helperBin.exists()) {
            std::string launchCmd = "\"" + helperBin.getFullPathName().toStdString() + "\" &";
            std::system(launchCmd.c_str());
            DBG("[M1SystemHelperManager] Launched directly: " + helperBin.getFullPathName());
            return true;
        }
        
        DBG("[M1SystemHelperManager] Could not find or launch helper on Linux");
        return false;
    }
#endif

    mutable std::mutex m_mutex;
    std::set<std::string> m_activeInstances;
    std::atomic<bool> m_launchFailed{false};
};

} // namespace Mach1

