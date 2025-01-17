//
//  WindowUtil.cpp
//  M1-Panner
//
#ifdef WIN32

#include "WindowUtil.h"
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

float WindowUtil::x = 0;
float WindowUtil::y = 0;
float WindowUtil::width = 0;
float WindowUtil::height = 0;

bool WindowUtil::isFound = false;
bool WindowUtil::isBusy = false;

// Initialize the static vector with video player names
std::vector<std::string> WindowUtil::videoPlayerNames = {
    "Avid Video Engine",
    "Video Engine",
    "Video",
    "FL Studio Video Player",
    "Logic Pro Video",
    "Studio One Video Player",
    "Cubase Video Player"
};

// Helper function to convert wide string to string
std::string WideStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Windows callback function for EnumWindows
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    // Get window title
    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title)/sizeof(wchar_t));
    std::string windowTitle = WideStringToString(std::wstring(title));

    // Check if window matches any of our video player names
    for (const auto& videoPlayerName : WindowUtil::videoPlayerNames) {
        if (windowTitle.find(videoPlayerName) != std::string::npos) {
            // Get window rect
            RECT rect;
            DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));

            // Update WindowUtil static members
            WindowUtil::x = static_cast<float>(rect.left);
            WindowUtil::y = static_cast<float>(rect.top) + 15;  // Adding 15 to match macOS behavior
            WindowUtil::width = static_cast<float>(rect.right - rect.left);
            WindowUtil::height = static_cast<float>(rect.bottom - rect.top) - 15;  // Subtracting 15 to match macOS behavior
            WindowUtil::isFound = true;
            return FALSE;  // Stop enumeration since we found our window
        }
    }

    return TRUE;  // Continue enumeration
}

void WindowUtil::update() {
    if (!WindowUtil::isBusy) {
        WindowUtil::isBusy = true;
        WindowUtil::isFound = false;

        // Enumerate all windows
        EnumWindows(EnumWindowsProc, 0);

        WindowUtil::isBusy = false;
    }
}

#endif // WIN32
