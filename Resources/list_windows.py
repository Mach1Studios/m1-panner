# pip install pyobjc-framework-Quartz
# python list_windows.py

from Quartz import (
    CGWindowListCopyWindowInfo,
    kCGWindowListOptionOnScreenOnly,
    kCGNullWindowID,
    kCGWindowName,
    kCGWindowOwnerName
)

def list_windows():
    # Get window list
    window_list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID)

    # Print header
    print("\nActive Windows:")
    print("-" * 80)
    print(f"{'Application Name':<40} | {'Window Name':<40}")
    print("-" * 80)

    # Iterate through windows
    for window in window_list:
        owner_name = window.get(kCGWindowOwnerName, "Unknown")
        window_name = window.get(kCGWindowName, "")

        # Only print if we have an owner name
        if owner_name:
            print(f"{str(owner_name):<40} | {str(window_name):<40}")

if __name__ == "__main__":
    list_windows()
