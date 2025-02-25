# Overlay Debug Tools

This directory contains tools for debugging the video window overlay functionality of the M1-Panner plugin.

## Video Window Simulator

The video window simulator (`video_window_simulator.py`) creates a test window that simulates a DAW's video player window. This helps debug the overlay window detection and tracking functionality.

### Requirements
- Python 3.x
- tkinter (usually comes with Python)

### Usage

From the project root directory:

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

### Supported Window Titles

The plugin looks for windows with these titles:
- "Avid Video Engine"
- "Video Engine"
- "Video"
- "Video Player"
- "FL Studio Video Player"
- "Logic Pro Video"
- "Studio One Video Player"
- "Cubase Video Player"

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
- Check if the window title exactly matches one in the supported list
- Monitor the simulator's position/size display vs the overlay position
- Try different window titles to test the detection logic
