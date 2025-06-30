#!/bin/bash

# Default values
TITLE="Video Player"
WIDTH=640
HEIGHT=480

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --title)
        TITLE="$2"
        shift
        shift
        ;;
        --width)
        WIDTH="$2"
        shift
        shift
        ;;
        --height)
        HEIGHT="$2"
        shift
        shift
        ;;
        *)
        echo "Unknown option: $1"
        exit 1
        ;;
    esac
done

# Run the Python script
python3 video_window_simulator.py --title "$TITLE" --width $WIDTH --height $HEIGHT
