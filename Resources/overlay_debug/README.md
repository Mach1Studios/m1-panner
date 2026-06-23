# Overlay Debug Tools

This directory contains tools for debugging the video window overlay functionality of the M1-Panner plugin.

## Window Lister

The window lister (`list_windows.py`) prints on-screen macOS windows using the same filters and title/owner matching as `WindowUtil.mm`. Use it to discover the exact window title or owner a DAW uses for its video player.

### Requirements
- Python 3.x
- pyobjc-framework-Quartz (`pip3 install pyobjc-framework-Quartz`)
- Screen Recording permission (System Settings → Privacy & Security → Screen Recording) if window titles appear blank

### Usage

From this directory:

```bash
# List windows the plugin can see (default)
./run_list_windows.sh

# Only show known DAW video window title or owner matches
./run_list_windows.sh --video-only

# Search for a DAW or window title substring
./run_list_windows.sh --grep "Pro Tools"
./run_list_windows.sh --grep "Video"

# Include windows the plugin ignores (sharing disabled, etc.)
./run_list_windows.sh --all
```

From the project root:

```bash
make overlay-debug-list
make overlay-debug-list ARGS='--grep "Logic"'
```

### Command Line Arguments

- `--video-only`: Only show windows whose title or owner matches a known video player name
- `--grep TEXT`: Filter by app or window title substring (case-insensitive)
- `--all`: Include windows hidden from the plugin
- `--no-highlight`: Do not mark known video player matches with `*`

When a match is found, the tool also prints the overlay bounds the plugin would use (`y + 15`, `height - 15`).
Known matches smaller than `100 x 100` are ignored so menu-bar and control-center items with names like `AudioVideoModule` are not treated as video windows.

## Video Window Simulator

The video window simulator (`video_window_simulator.py`) creates a test window that simulates a DAW's video player window. This helps debug the overlay window detection and tracking functionality.

### Requirements
- Python 3.x
- tkinter (usually comes with Python)

### Usage

From this directory:

```bash
# Basic usage with defaults
./run_simulator.sh

# Specify a custom title
./run_simulator.sh --title "Avid Video Engine"

# Specify size
./run_simulator.sh --width 800 --height 600

# Specify both title and size
./run_simulator.sh --title "Video Player" --width 1024 --height 768
```

### Command Line Arguments

- `--title`: Window title (default: "Video Player")
- `--width`: Initial window width in pixels (default: 640)
- `--height`: Initial window height in pixels (default: 480)

### Supported Window Titles And Owners

The plugin looks for windows with these titles:
- "Avid Video Engine"
- "Video Engine"
- "Video"
- "Video Player"
- "FL Studio Video Player"
- "Logic Pro Video"
- "Studio One Video Player"
- "Cubase Video Player"

The plugin also matches these macOS window owners, which catches newer Pro Tools video windows that report an empty title:
- "Avid Video Engine"

### Features

- Creates a resizable window with customizable title
- Displays real-time position and size information
- Can be moved and resized to test overlay tracking
- Lightweight alternative to running a full DAW for testing

### Debugging Tips

1. Launch the simulator with one of the supported window titles
2. Open your plugin in a DAW or test host
3. Enable the overlay feature in the plugin
4. Move and resize the simulator window
5. Verify that the overlay properly tracks the simulator window

If the overlay isn't tracking properly:
- Check if the window title or owner matches one in the supported list
- Monitor the simulator's position/size display vs the overlay position
- Try different window titles to test the detection logic
