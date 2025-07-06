#!/bin/bash

# M1MemoryShare Test Script
# This script tests the shared memory functionality by monitoring memory files created by M1Panner instances

set -e

# Check multiple possible locations for shared memory files
TEMP_DIRS=(
    "$HOME/Library/Caches/M1-Panner"
    "/tmp"
    "$HOME/Library/Caches"
)

MEMORY_FILE_PATTERN="M1SpatialSystem_M1Panner_PID*_PTR*_T*.mem"

echo "=========================================="
echo "M1MemoryShare Test Script"
echo "=========================================="
echo "Monitoring shared memory files for M1Panner instances..."
echo "Checking directories:"
for dir in "${TEMP_DIRS[@]}"; do
    echo "  - $dir"
done
echo "Expected file patterns:"
echo "  - Audio data with enhanced headers: $MEMORY_FILE_PATTERN"
echo "  - (Headers now include panner settings and DAW timestamp)"
echo ""

# Function to cleanup on exit
cleanup() {
    echo "Cleaning up test files..."
    rm -f /tmp/test_audio_reader /tmp/test_control_reader
    echo "Test script terminated."
}
trap cleanup EXIT

# Function to monitor memory files
monitor_memory_files() {
    echo "Scanning for shared memory files..."

    # Look for audio memory files in all directories
    audio_files=()

    for temp_dir in "${TEMP_DIRS[@]}"; do
        if [ -d "$temp_dir" ]; then
            audio_files+=($(ls "$temp_dir"/$MEMORY_FILE_PATTERN 2>/dev/null || true))
        fi
    done

    if [ ${#audio_files[@]} -eq 0 ]; then
        echo "No M1Panner shared memory files found."
        echo "Please ensure:"
        echo "1. M1Panner plugin is loaded in your DAW"
        echo "2. external_spatialmixer_active flag is set to true"
        echo "3. Plugin is configured as 1-channel (mono) or 2-channel (stereo)"
        echo "4. Audio is playing through the plugin"
        return 1
    fi

    echo "Found ${#audio_files[@]} audio memory file(s) with enhanced headers"

    return 0
}

# Function to display file information
display_file_info() {
    local file=$1
    local type=$2

    if [ -f "$file" ]; then
        local size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null || echo "unknown")
        local modified=$(stat -f%Sm "$file" 2>/dev/null || stat -c%y "$file" 2>/dev/null || echo "unknown")
        echo "[$type] $file"
        echo "    Size: $size bytes"
        echo "    Modified: $modified"

        # Try to read first few bytes as hex
        if [ "$size" != "unknown" ] && [ "$size" -gt 0 ]; then
            echo "    First 64 bytes (hex):"
            hexdump -C "$file" | head -4 | sed 's/^/      /'
        fi
        echo ""
    fi
}

# Function to create ASCII waveform visualization
create_waveform_display() {
    local samples=("$@")
    local width=80
    local height=20
    local waveform=""

    if [ ${#samples[@]} -eq 0 ]; then
        echo "No audio samples to display"
        return
    fi

    # Find min and max values for scaling
    local min_val=0
    local max_val=0
    for sample in "${samples[@]}"; do
        if (( $(echo "$sample < $min_val" | bc -l) )); then
            min_val=$sample
        fi
        if (( $(echo "$sample > $max_val" | bc -l) )); then
            max_val=$sample
        fi
    done

    # Avoid division by zero
    local range=$(echo "$max_val - $min_val" | bc -l)
    if (( $(echo "$range == 0" | bc -l) )); then
        range=1
    fi

    echo "┌$(printf '─%.0s' $(seq 1 $width))┐"

    # Create waveform lines
    for ((y=height-1; y>=0; y--)); do
        local line="│"
        local threshold=$(echo "scale=6; $min_val + ($range * $y / ($height - 1))" | bc -l)

        for ((x=0; x<width; x++)); do
            local sample_idx=$(echo "scale=0; $x * ${#samples[@]} / $width" | bc)
            if [ $sample_idx -ge ${#samples[@]} ]; then
                sample_idx=$((${#samples[@]} - 1))
            fi

            local sample_val=${samples[$sample_idx]}

            # Check if sample crosses this threshold line
            if (( $(echo "$sample_val >= $threshold" | bc -l) )); then
                if [ $y -eq $((height/2)) ]; then
                    line+="━"  # Center line
                else
                    line+="█"  # Waveform
                fi
            else
                if [ $y -eq $((height/2)) ]; then
                    line+="─"  # Center line
                else
                    line+=" "  # Empty space
                fi
            fi
        done
        line+="│"
        echo "$line"
    done

    echo "└$(printf '─%.0s' $(seq 1 $width))┘"
    printf "Min: %8.4f                                    Max: %8.4f\n" "$min_val" "$max_val"
}

# Function to try reading audio data from memory file
read_audio_data() {
    local file=$1
    echo "Attempting to read audio data structure from: $file"

    # Create a simple C program to read the memory structure
    cat > /tmp/test_audio_reader.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct SharedMemoryHeader {
    volatile uint32_t writeIndex;
    volatile uint32_t readIndex;
    volatile uint32_t dataSize;
    volatile uint32_t hasData;
    uint32_t bufferSize;
    uint32_t sampleRate;
    uint32_t numChannels;
    uint32_t samplesPerBlock;
    char name[64];
};

struct AudioBufferHeader {
    uint32_t channels;
    uint32_t samples;
    uint64_t dawTimestamp;
    double playheadPositionInSeconds;
    uint32_t isPlaying;

    // Panner settings
    float azimuth;
    float elevation;
    float diverge;
    float gain;
    float stereoOrbitAzimuth;
    float stereoSpread;
    float stereoInputBalance;
    uint32_t autoOrbit;
    uint32_t isotropicMode;
    uint32_t equalpowerMode;
    uint32_t gainCompensationMode;
    uint32_t inputMode;
    uint32_t outputMode;

    // Reserved
    uint32_t reserved[8];
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <memory_file>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Failed to open memory file");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("Failed to get file stats");
        close(fd);
        return 1;
    }

    void* mapped = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("Failed to map memory");
        close(fd);
        return 1;
    }

    struct SharedMemoryHeader* header = (struct SharedMemoryHeader*)mapped;

    printf("=== Memory Header Info ===\n");
    printf("Name: %s\n", header->name);
    printf("Buffer Size: %u bytes\n", header->bufferSize);
    printf("Sample Rate: %u Hz\n", header->sampleRate);
    printf("Channels: %u\n", header->numChannels);
    printf("Samples Per Block: %u\n", header->samplesPerBlock);
    printf("Data Size: %u bytes\n", header->dataSize);
    printf("Has Data: %s\n", header->hasData ? "Yes" : "No");
    printf("Write Index: %u\n", header->writeIndex);
    printf("Read Index: %u\n", header->readIndex);

    if (header->hasData && header->dataSize > sizeof(struct AudioBufferHeader)) {
        uint8_t* dataBuffer = (uint8_t*)mapped + sizeof(struct SharedMemoryHeader);
        struct AudioBufferHeader* audioHeader = (struct AudioBufferHeader*)dataBuffer;

        printf("\n=== Enhanced Audio Data Info ===\n");
        printf("Audio Channels: %u\n", audioHeader->channels);
        printf("Audio Samples: %u\n", audioHeader->samples);
        printf("DAW Timestamp: %llu ms\n", audioHeader->dawTimestamp);
        printf("Playhead Position: %.6f seconds\n", audioHeader->playheadPositionInSeconds);
        printf("Is Playing: %s\n", audioHeader->isPlaying ? "Yes" : "No");

        printf("\n=== Panner Settings ===\n");
        printf("Azimuth: %.2f degrees\n", audioHeader->azimuth);
        printf("Elevation: %.2f degrees\n", audioHeader->elevation);
        printf("Diverge: %.2f\n", audioHeader->diverge);
        printf("Gain: %.2f dB\n", audioHeader->gain);
        printf("Stereo Orbit Azimuth: %.2f degrees\n", audioHeader->stereoOrbitAzimuth);
        printf("Stereo Spread: %.2f\n", audioHeader->stereoSpread);
        printf("Stereo Input Balance: %.2f\n", audioHeader->stereoInputBalance);
        printf("Auto Orbit: %s\n", audioHeader->autoOrbit ? "On" : "Off");
        printf("Isotropic Mode: %s\n", audioHeader->isotropicMode ? "On" : "Off");
        printf("Equal Power Mode: %s\n", audioHeader->equalpowerMode ? "On" : "Off");
        printf("Gain Compensation: %s\n", audioHeader->gainCompensationMode ? "On" : "Off");
        printf("Input Mode: %u\n", audioHeader->inputMode);
        printf("Output Mode: %u\n", audioHeader->outputMode);

        if (audioHeader->channels > 0 && audioHeader->samples > 0 && audioHeader->channels <= 8 && audioHeader->samples <= 8192) {
            float* audioData = (float*)(dataBuffer + sizeof(struct AudioBufferHeader));
            printf("\nFirst few audio samples:\n");
            for (int i = 0; i < 10 && i < (audioHeader->samples * audioHeader->channels); i++) {
                printf("  Sample %d: %.6f\n", i, audioData[i]);
            }

            // Output samples for waveform visualization (first 200 samples, channel 0 only)
            printf("\nWAVEFORM_DATA_START\n");
            int max_samples = (audioHeader->samples < 200) ? audioHeader->samples : 200;
            for (int i = 0; i < max_samples; i++) {
                // Get sample from first channel (interleaved format: ch0, ch1, ch0, ch1, ...)
                float sample = audioData[i * audioHeader->channels];
                printf("%.6f\n", sample);
            }
            printf("WAVEFORM_DATA_END\n");
        }
    }

    munmap(mapped, st.st_size);
    close(fd);
    return 0;
}
EOF

    # Compile and run the reader
    if gcc -o /tmp/test_audio_reader /tmp/test_audio_reader.c 2>/dev/null; then
        local output=$(/tmp/test_audio_reader "$file")
        echo "$output"

        # Extract waveform data and create visualization
        local waveform_data=$(echo "$output" | sed -n '/WAVEFORM_DATA_START/,/WAVEFORM_DATA_END/p' | grep -v 'WAVEFORM_DATA_')
        if [ ! -z "$waveform_data" ]; then
            echo ""
            echo "🎵 Audio Waveform Visualization (First 200 samples, Channel 0):"
            echo ""

            # Convert to array and display waveform (check if bc is available)
            if command -v bc >/dev/null 2>&1; then
                local samples_array=()
                while IFS= read -r line; do
                    if [ ! -z "$line" ]; then
                        samples_array+=("$line")
                    fi
                done <<< "$waveform_data"

                if [ ${#samples_array[@]} -gt 0 ]; then
                    create_waveform_display "${samples_array[@]}"
                else
                    echo "No valid audio samples found for waveform display"
                fi
                         else
                echo "bc command not available - using simple waveform visualization"
                # Simple waveform without bc - just show relative amplitude
                local samples_array=()
                while IFS= read -r line; do
                    if [ ! -z "$line" ]; then
                        samples_array+=("$line")
                    fi
                done <<< "$waveform_data"

                if [ ${#samples_array[@]} -gt 0 ]; then
                    echo "Simple Audio Level Display (first 40 samples):"
                    local count=0
                    for sample in "${samples_array[@]}"; do
                        if [ $count -ge 40 ]; then break; fi

                        # Convert to integer representation for simple display
                        local int_val=$(printf "%.0f" $(echo "$sample * 10" | awk '{print $1}'))
                        local abs_val=${int_val#-}  # Remove negative sign

                        if [ $abs_val -gt 10 ]; then abs_val=10; fi
                        if [ $abs_val -lt 0 ]; then abs_val=0; fi

                        local bar=""
                        for ((i=0; i<abs_val; i++)); do
                            bar+="█"
                        done

                        printf "%3d: %-10s %s\n" $count "$bar" "$sample"
                        count=$((count + 1))
                    done
                else
                    echo "No valid audio samples found for simple waveform display"
                fi
            fi
        else
            echo "No waveform data extracted from audio file"
        fi
    else
        echo "Could not compile audio reader. Showing raw hex dump instead:"
        echo "First 256 bytes:"
        hexdump -C "$file" | head -16 | sed 's/^/  /'
    fi
    echo ""
}

# Function to read control data - REMOVED since control data is now in audio buffer headers

# Main monitoring loop
echo "Starting monitoring loop (Press Ctrl+C to stop)..."
echo ""

iteration=0
while true; do
    iteration=$((iteration + 1))
    echo "=== Check #$iteration at $(date) ==="

    if monitor_memory_files; then
        # Show detailed analysis every 3rd iteration to avoid spam
        if [ $((iteration % 3)) -eq 0 ]; then
            echo "Detailed Analysis (every 3rd check):"
            echo ""

            # Process first audio file for detailed analysis
            if [ ${#audio_files[@]} -gt 0 ]; then
                echo "Analyzing most recent audio file with enhanced headers..."
                display_file_info "${audio_files[0]}" "AUDIO_WITH_ENHANCED_HEADERS"
                read_audio_data "${audio_files[0]}"
            fi
        else
            echo "Memory sharing is active! Audio data should be changing if audio is playing."
            echo "Found ${#audio_files[@]} audio file(s) with enhanced headers containing panner settings and DAW timestamp"
            echo "(Detailed waveform analysis shown every 3rd check)"
        fi
    else
        echo "No active memory sharing detected."
        echo ""
        echo "To activate memory sharing:"
        echo "1. Load M1Panner plugin in your DAW"
        echo "2. Set external_spatialmixer_active = true in the plugin"
        echo "3. Configure as mono (1,2) or stereo (2,2) channel layout"
        echo "4. Play audio through the plugin"
    fi

    echo "Waiting 3 seconds for next check..."
    echo "=========================================="
    echo ""

    sleep 3
done
