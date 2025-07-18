#!/bin/bash

# M1MemoryShare Timeline Consumer Test Script
# This script acts as a consumer of the enhanced M1MemoryShare system
# and creates a timeline visualization of audio buffers based on playhead position and timestamps

set -e

# Configuration
CONSUMER_ID=9001
MEMORY_FILE_PATTERN="M1SpatialSystem_M1Panner_PID*_PTR*_T*.mem"
TIMELINE_LOG_FILE="/tmp/m1_timeline_consumer.log"
CONSUMED_BUFFERS_FILE="/tmp/m1_consumed_buffers.json"
MISSING_BUFFERS_FILE="/tmp/m1_missing_buffers.json"

# Check multiple possible locations for shared memory files
TEMP_DIRS=(
    "$HOME/Library/Caches/M1-Panner"
    "/tmp"
    "$HOME/Library/Caches"
)

echo "=========================================="
echo "M1MemoryShare Timeline Consumer Test"
echo "=========================================="
echo "Consumer ID: $CONSUMER_ID"
echo "Timeline Log: $TIMELINE_LOG_FILE"
echo "Consumed Buffers: $CONSUMED_BUFFERS_FILE"
echo "Missing Buffers: $MISSING_BUFFERS_FILE"
echo ""

# Initialize log files
echo "# M1MemoryShare Timeline Consumer Log" > "$TIMELINE_LOG_FILE"
echo "# Format: timestamp,buffer_id,sequence_number,playhead_position,daw_timestamp,is_playing,consumed_status" >> "$TIMELINE_LOG_FILE"
echo "{\"consumed_buffers\": [], \"session_start\": \"$(date -Iseconds)\"}" > "$CONSUMED_BUFFERS_FILE"
echo "{\"missing_buffers\": [], \"session_start\": \"$(date -Iseconds)\"}" > "$MISSING_BUFFERS_FILE"

# Function to cleanup on exit
cleanup() {
    echo "Cleaning up consumer files..."
    rm -f /tmp/timeline_consumer_reader
    echo "Timeline consumer terminated."
}
trap cleanup EXIT

# Function to find shared memory files
find_memory_files() {
    local audio_files=()

    for temp_dir in "${TEMP_DIRS[@]}"; do
        if [ -d "$temp_dir" ]; then
            audio_files+=($(ls "$temp_dir"/$MEMORY_FILE_PATTERN 2>/dev/null || true))
        fi
    done

    echo "${audio_files[@]}"
}

# Function to create C program for timeline consumer
create_timeline_consumer() {
    cat > /tmp/timeline_consumer_reader.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

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
    // Enhanced buffer queue management
    volatile uint32_t queueSize;
    volatile uint32_t maxQueueSize;
    volatile uint32_t nextSequenceNumber;
    volatile uint64_t nextBufferId;
    // Consumer management
    volatile uint32_t consumerCount;
    volatile uint32_t consumerIds[16];
};

struct QueuedBuffer {
    uint64_t bufferId;
    uint32_t sequenceNumber;
    uint64_t timestamp;
    uint32_t dataSize;
    uint32_t dataOffset;
    uint32_t requiresAcknowledgment;
    uint32_t consumerCount;
    uint32_t acknowledgedCount;
    uint32_t consumerIds[16];
    uint32_t acknowledged[16];
};

struct GenericAudioBufferHeader {
    uint32_t version;
    uint32_t channels;
    uint32_t samples;
    uint64_t dawTimestamp;
    double playheadPositionInSeconds;
    uint32_t isPlaying;
    uint32_t parameterCount;
    uint32_t headerSize;
    uint32_t updateSource;
    uint32_t isUpdatingFromExternal;
    // Enhanced buffer acknowledgment fields
    uint64_t bufferId;
    uint32_t sequenceNumber;
    uint64_t bufferTimestamp;
    uint32_t requiresAcknowledgment;
    uint32_t consumerCount;
    uint32_t acknowledgedCount;
    uint32_t reserved[2];
};

void print_timeline_entry(uint64_t bufferId, uint32_t sequenceNumber, double playheadPosition,
                         uint64_t dawTimestamp, bool isPlaying, const char* status) {
    time_t currentTime = time(NULL);
    printf("TIMELINE_ENTRY:%ld,%llu,%u,%.6f,%llu,%s,%s\n",
           currentTime, bufferId, sequenceNumber, playheadPosition,
           dawTimestamp, isPlaying ? "PLAYING" : "STOPPED", status);
}

bool is_consumer_registered(struct SharedMemoryHeader* header, uint32_t consumerId) {
    for (uint32_t i = 0; i < header->consumerCount && i < 16; i++) {
        if (header->consumerIds[i] == consumerId) {
            return true;
        }
    }
    return false;
}

void analyze_buffer_queue(struct SharedMemoryHeader* header, struct QueuedBuffer* queuedBuffers,
                         uint32_t consumerId) {
    printf("QUEUE_ANALYSIS_START\n");
    printf("Queue Size: %u\n", header->queueSize);
    printf("Max Queue Size: %u\n", header->maxQueueSize);
    printf("Next Sequence Number: %u\n", header->nextSequenceNumber);
    printf("Next Buffer ID: %llu\n", header->nextBufferId);

    uint32_t consumedCount = 0;
    uint32_t unconsumedCount = 0;
    uint32_t missingFromSequence = 0;
    uint32_t expectedSequence = 0;

    for (uint32_t i = 0; i < header->queueSize; i++) {
        struct QueuedBuffer* buffer = &queuedBuffers[i];

        if (buffer->bufferId == 0) continue; // Skip empty slots

        // Check if this consumer has acknowledged this buffer
        bool acknowledged = false;
        for (uint32_t j = 0; j < buffer->consumerCount; j++) {
            if (buffer->consumerIds[j] == consumerId) {
                acknowledged = buffer->acknowledged[j];
                break;
            }
        }

        if (acknowledged) {
            consumedCount++;
        } else {
            unconsumedCount++;
        }

        // Check for sequence gaps
        if (buffer->sequenceNumber != expectedSequence) {
            missingFromSequence += (buffer->sequenceNumber - expectedSequence);
        }
        expectedSequence = buffer->sequenceNumber + 1;

        printf("BUFFER_QUEUE_ENTRY:%llu,%u,%llu,%s,%u,%u\n",
               buffer->bufferId, buffer->sequenceNumber, buffer->timestamp,
               acknowledged ? "CONSUMED" : "UNCONSUMED",
               buffer->consumerCount, buffer->acknowledgedCount);
    }

    printf("QUEUE_SUMMARY:consumed=%u,unconsumed=%u,missing_from_sequence=%u\n",
           consumedCount, unconsumedCount, missingFromSequence);
    printf("QUEUE_ANALYSIS_END\n");
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <memory_file> <consumer_id>\n", argv[0]);
        return 1;
    }

    const char* memoryFile = argv[1];
    uint32_t consumerId = atoi(argv[2]);

    int fd = open(memoryFile, O_RDWR);
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

    void* mapped = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("Failed to map memory");
        close(fd);
        return 1;
    }

    struct SharedMemoryHeader* header = (struct SharedMemoryHeader*)mapped;

    // Check if consumer is registered
    if (!is_consumer_registered(header, consumerId)) {
        printf("CONSUMER_NOT_REGISTERED:%u\n", consumerId);
        munmap(mapped, st.st_size);
        close(fd);
        return 1;
    }

    // Calculate queued buffers location
    struct QueuedBuffer* queuedBuffers = (struct QueuedBuffer*)(
        (uint8_t*)mapped + sizeof(struct SharedMemoryHeader)
    );

    // Analyze buffer queue
    analyze_buffer_queue(header, queuedBuffers, consumerId);

    // Read current audio data if available
    if (header->hasData && header->dataSize > sizeof(struct GenericAudioBufferHeader)) {
        size_t queuedBuffersSize = header->maxQueueSize * sizeof(struct QueuedBuffer);
        uint8_t* dataBuffer = (uint8_t*)mapped + sizeof(struct SharedMemoryHeader) + queuedBuffersSize;
        struct GenericAudioBufferHeader* audioHeader = (struct GenericAudioBufferHeader*)dataBuffer;

        // Print timeline entry for current buffer
        print_timeline_entry(audioHeader->bufferId, audioHeader->sequenceNumber,
                            audioHeader->playheadPositionInSeconds, audioHeader->dawTimestamp,
                            audioHeader->isPlaying, "AVAILABLE");

        printf("CURRENT_BUFFER_INFO:%llu,%u,%.6f,%llu,%s,%s\n",
               audioHeader->bufferId, audioHeader->sequenceNumber,
               audioHeader->playheadPositionInSeconds, audioHeader->dawTimestamp,
               audioHeader->isPlaying ? "PLAYING" : "STOPPED",
               audioHeader->requiresAcknowledgment ? "REQUIRES_ACK" : "NO_ACK");
    }

    munmap(mapped, st.st_size);
    close(fd);
    return 0;
}
EOF

    # Compile the consumer reader
    if gcc -o /tmp/timeline_consumer_reader /tmp/timeline_consumer_reader.c 2>/dev/null; then
        return 0
    else
        echo "Failed to compile timeline consumer reader"
        return 1
    fi
}

# Function to process timeline entries
process_timeline_entries() {
    local output="$1"
    local consumed_count=0
    local missing_count=0

    echo "Processing timeline entries..."

    # Extract timeline entries
    echo "$output" | grep "^TIMELINE_ENTRY:" | while IFS=',' read -r prefix timestamp buffer_id sequence_number playhead_position daw_timestamp is_playing status; do
        # Remove prefix
        timestamp=${timestamp#*:}

        # Create timeline visualization
        local playhead_bar=""
        local bar_length=50
        local position_percent=$(echo "scale=2; $playhead_position / 10.0" | bc -l 2>/dev/null || echo "0")
        local bar_pos=$(echo "scale=0; $position_percent * $bar_length / 100" | bc -l 2>/dev/null || echo "0")

        if [ "$bar_pos" -gt 0 ] && [ "$bar_pos" -le "$bar_length" ]; then
            for ((i=0; i<bar_pos; i++)); do
                playhead_bar+="█"
            done
            for ((i=bar_pos; i<bar_length; i++)); do
                playhead_bar+="─"
            done
        else
            playhead_bar=$(printf '─%.0s' $(seq 1 $bar_length))
        fi

        # Format timestamp
        local formatted_time=$(date -d "@$timestamp" '+%H:%M:%S' 2>/dev/null || echo "$timestamp")

        echo "[$formatted_time] Buffer $buffer_id (seq:$sequence_number) [$playhead_bar] ${playhead_position}s [$status]"

        # Log to file
        echo "$timestamp,$buffer_id,$sequence_number,$playhead_position,$daw_timestamp,$is_playing,$status" >> "$TIMELINE_LOG_FILE"
    done

    # Process queue analysis
    echo ""
    echo "Queue Analysis:"
    echo "$output" | grep "^QUEUE_SUMMARY:" | while IFS=',' read -r prefix consumed_info unconsumed_info missing_info; do
        consumed_count=${consumed_info#*=}
        unconsumed_count=${unconsumed_info#*=}
        missing_count=${missing_info#*=}

        echo "  Consumed Buffers: $consumed_count"
        echo "  Unconsumed Buffers: $unconsumed_count"
        echo "  Missing from Sequence: $missing_count"

        # Update JSON files
        jq ".consumed_buffers += [\"$(date -Iseconds): $consumed_count buffers consumed\"]" "$CONSUMED_BUFFERS_FILE" > /tmp/consumed_temp.json && mv /tmp/consumed_temp.json "$CONSUMED_BUFFERS_FILE"
        jq ".missing_buffers += [\"$(date -Iseconds): $missing_count buffers missing\"]" "$MISSING_BUFFERS_FILE" > /tmp/missing_temp.json && mv /tmp/missing_temp.json "$MISSING_BUFFERS_FILE"
    done

    # Process individual buffer entries
    echo ""
    echo "Buffer Queue Details:"
    echo "$output" | grep "^BUFFER_QUEUE_ENTRY:" | while IFS=',' read -r prefix buffer_id sequence_number timestamp status consumer_count ack_count; do
        buffer_id=${buffer_id#*:}
        local timestamp_formatted=$(date -d "@$((timestamp/1000))" '+%H:%M:%S' 2>/dev/null || echo "$timestamp")

        local status_symbol="?"
        if [ "$status" = "CONSUMED" ]; then
            status_symbol="✓"
        elif [ "$status" = "UNCONSUMED" ]; then
            status_symbol="⏳"
        fi

        echo "  $status_symbol Buffer $buffer_id (seq:$sequence_number) @ $timestamp_formatted - $ack_count/$consumer_count acks"
    done
}

# Function to create timeline visualization
create_timeline_visualization() {
    local log_file="$1"
    echo ""
    echo "📊 Timeline Visualization (Last 20 entries):"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    if [ -f "$log_file" ]; then
        tail -n 20 "$log_file" | while IFS=',' read -r timestamp buffer_id sequence_number playhead_position daw_timestamp is_playing status; do
            if [ "$timestamp" != "# M1MemoryShare Timeline Consumer Log" ] && [ "$timestamp" != "# Format: timestamp,buffer_id,sequence_number,playhead_position,daw_timestamp,is_playing,consumed_status" ]; then
                local formatted_time=$(date -d "@$timestamp" '+%H:%M:%S' 2>/dev/null || echo "$timestamp")
                local status_symbol="?"

                case "$status" in
                    "CONSUMED") status_symbol="✅" ;;
                    "UNCONSUMED") status_symbol="⏳" ;;
                    "AVAILABLE") status_symbol="🎵" ;;
                    "MISSING") status_symbol="❌" ;;
                    *) status_symbol="?" ;;
                esac

                # Create playhead position bar
                local playhead_percent=$(echo "scale=0; $playhead_position * 100 / 10" | bc -l 2>/dev/null || echo "0")
                local bar_length=30
                local filled_length=$(echo "scale=0; $playhead_percent * $bar_length / 100" | bc -l 2>/dev/null || echo "0")

                local playhead_bar=""
                for ((i=0; i<filled_length && i<bar_length; i++)); do
                    playhead_bar+="█"
                done
                for ((i=filled_length; i<bar_length; i++)); do
                    playhead_bar+="─"
                done

                printf "%s %s Buffer %s (seq:%s) [%s] %6.2fs %s %s\n" \
                       "$formatted_time" "$status_symbol" "$buffer_id" "$sequence_number" \
                       "$playhead_bar" "$playhead_position" "$is_playing" "$status"
            fi
        done
    else
        echo "No timeline data available yet."
    fi

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

# Function to display buffer statistics
display_buffer_statistics() {
    echo ""
    echo "📈 Buffer Statistics:"

    if [ -f "$CONSUMED_BUFFERS_FILE" ]; then
        local consumed_count=$(jq '.consumed_buffers | length' "$CONSUMED_BUFFERS_FILE" 2>/dev/null || echo "0")
        echo "  Total consumption events: $consumed_count"
    fi

    if [ -f "$MISSING_BUFFERS_FILE" ]; then
        local missing_count=$(jq '.missing_buffers | length' "$MISSING_BUFFERS_FILE" 2>/dev/null || echo "0")
        echo "  Missing buffer events: $missing_count"
    fi

    if [ -f "$TIMELINE_LOG_FILE" ]; then
        local total_entries=$(wc -l < "$TIMELINE_LOG_FILE" 2>/dev/null || echo "0")
        local consumed_entries=$(grep -c "CONSUMED" "$TIMELINE_LOG_FILE" 2>/dev/null || echo "0")
        local unconsumed_entries=$(grep -c "UNCONSUMED" "$TIMELINE_LOG_FILE" 2>/dev/null || echo "0")

        echo "  Total timeline entries: $total_entries"
        echo "  Consumed entries: $consumed_entries"
        echo "  Unconsumed entries: $unconsumed_entries"

        if [ "$total_entries" -gt 0 ]; then
            local consumed_percent=$(echo "scale=1; $consumed_entries * 100 / $total_entries" | bc -l 2>/dev/null || echo "0")
            echo "  Consumption rate: $consumed_percent%"
        fi
    fi
}

# Main execution
echo "Creating timeline consumer reader..."
if ! create_timeline_consumer; then
    echo "Failed to create timeline consumer reader"
    exit 1
fi

echo "Starting timeline consumer monitoring loop..."
echo "Press Ctrl+C to stop monitoring"
echo ""

iteration=0
while true; do
    iteration=$((iteration + 1))
    echo "=== Timeline Consumer Check #$iteration at $(date) ==="

    # Find memory files
    memory_files=($(find_memory_files))

    if [ ${#memory_files[@]} -eq 0 ]; then
        echo "No M1Panner shared memory files found."
        echo "Please ensure M1Panner is running with external_spatialmixer_active=true"
    else
        echo "Found ${#memory_files[@]} memory file(s). Processing first file..."

        # Process the first memory file
        local output=$(/tmp/timeline_consumer_reader "${memory_files[0]}" "$CONSUMER_ID" 2>/dev/null)

        if [ $? -eq 0 ]; then
            process_timeline_entries "$output"
            create_timeline_visualization "$TIMELINE_LOG_FILE"
            display_buffer_statistics
        else
            echo "Failed to process memory file or consumer not registered"
            echo "Make sure the M1Panner plugin has registered consumer ID $CONSUMER_ID"
        fi
    fi

    echo ""
    echo "Next check in 2 seconds..."
    echo "=========================================="
    echo ""

    sleep 2
done
