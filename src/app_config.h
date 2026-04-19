#pragma once

#include <Arduino.h>



// IP address of the backend server that receives uplink audio
// and sends downlink audio back to the ESP32.
constexpr const char *SERVER_IP = "74.208.158.226";

// UDP port on the backend server.
constexpr uint16_t SERVER_PORT = 9999;

// Local UDP port used by the ESP32 to bind and receive packets.
constexpr uint16_t LOCAL_UDP_PORT = 12000;

// Recommendation:
// - Keep SERVER_PORT fixed unless the backend changes.
// - LOCAL_UDP_PORT can be changed if another service already uses it.
// - For production, consider moving WiFi credentials out of source code.

// ============================================================
// Audio common
// ============================================================

// Shared audio sample rate used by both uplink PCM and downlink Opus.
constexpr uint32_t SAMPLE_RATE_HZ = 16000;

// Number of audio channels.
// 1 = mono.
constexpr uint8_t AUDIO_CHANNELS = 1;

// Recommendation:
// - 16 kHz mono is a good balance for voice quality, bandwidth, and CPU usage.
// - Do not change this unless the full pipeline (ESP32, server, codec config)
//   is updated to match the same sample rate and channel count.

// ============================================================
// PCM uplink
// ============================================================

// Version number of the custom PCM uplink packet format.
constexpr uint8_t PCM_VERSION = 1;

// Flag used in uplink PCM packets to mark the end of a recorded turn.
constexpr uint8_t PCM_FLAG_END = 0x01;

// Number of PCM samples sent in each uplink packet.
// 320 samples at 16 kHz = 20 ms of audio.
constexpr uint16_t PCM_FRAME_SAMPLES = 320;

// Size in bytes of one uplink PCM audio frame.
// PCM16 mono = 2 bytes per sample.
constexpr size_t PCM_FRAME_BYTES = PCM_FRAME_SAMPLES * 2;

// Size in bytes of the custom uplink PCM packet header.
constexpr size_t PCM_HEADER_SIZE = 18;

// Maximum total uplink packet size = header + PCM payload.
constexpr size_t PCM_PACKET_MAX = PCM_HEADER_SIZE + PCM_FRAME_BYTES;

// Delay between the last PCM END packet and the COMMIT message.
// Gives the backend a short moment to receive the final audio packets first.
constexpr uint32_t COMMIT_DELAY_MS = 80;

// Number of times the END packet is repeated for reliability over UDP.
constexpr uint8_t PCM_END_REPEAT_COUNT = 3;

// Delay between repeated END packets.
constexpr uint32_t PCM_END_REPEAT_GAP_MS = 12;

// Recommendation:
// - 20 ms uplink packets are a safe and common choice for realtime voice.
// - Smaller packets reduce latency but increase packet overhead and CPU activity.
// - Larger packets reduce overhead but may increase latency and make losses more noticeable.
// - COMMIT_DELAY_MS should usually stay below 100 ms for responsive turn closing.
// - Repeating END packets is useful over UDP; keep 2-3 repeats unless you confirm
//   the network is extremely stable.

// ============================================================
// Opus downlink
// ============================================================

// Version number of the custom Opus downlink packet format.
constexpr uint8_t OPUS_VERSION = 1;

// Flag used in downlink Opus packets to mark the end of the response stream.
constexpr uint8_t OPUS_FLAG_END = 0x01;

// Number of decoded audio samples expected from each Opus packet.
// 320 samples at 16 kHz = 20 ms of audio.
constexpr uint16_t OPUS_FRAME_SAMPLES = 320;

// Maximum number of decoded PCM samples produced from one Opus frame.
constexpr uint16_t MAX_DECODE_SAMPLES = OPUS_FRAME_SAMPLES;

// Maximum allowed Opus payload size in bytes for one UDP packet.
constexpr size_t MAX_OPUS_PAYLOAD = 300;

// Size in bytes of the custom downlink Opus packet header.
constexpr size_t OPUS_HEADER_SIZE = 18;

// Recommendation:
// - Keep OPUS_FRAME_SAMPLES aligned with the encoder frame size used by the backend.
// - MAX_OPUS_PAYLOAD should be large enough for worst-case packets, but not excessively large.
// - If packet truncation appears in logs, increase MAX_OPUS_PAYLOAD carefully.
// - If you change OPUS_VERSION or packet layout, update both ESP32 and backend together.

// ============================================================
// Jitter buffer
// ============================================================

// Total number of packet slots reserved in the jitter buffer.
constexpr int JITTER_SLOTS = 170;

// Number of buffered packets required before playback starts the first time.
// Higher values improve smoothness but increase latency.
constexpr int START_BUFFERED_PACKETS = 50;

// Number of buffered packets required before resuming after underrun/rebuffer.
constexpr int REBUFFER_PACKETS = 40;

// Buffer threshold considered "low" during playback.
constexpr int LOW_WATER_PACKETS = 12;

// Maximum number of consecutive missing packets tolerated before
// stronger recovery or rebuffering logic is triggered.
constexpr int MAX_CONSECUTIVE_MISSES = 10;

// Largest acceptable future sequence jump relative to expectedSeq.
// Packets too far ahead are dropped to avoid runaway buffering.
constexpr uint32_t MAX_AHEAD_WINDOW = 100;

// If the sequence gap becomes larger than this during playback,
// the code may force a hard rebuffer.
constexpr uint32_t HARD_REBUFFER_GAP = 28;

// Number of consecutive misses required before enabling hard rebuffer logic.
constexpr uint32_t HARD_REBUFFER_MISSES = 10;

// Recommendation:
// - START_BUFFERED_PACKETS is one of the main latency vs smoothness controls.
// - Lower it for lower latency on stable local networks.
// - Raise it for unstable remote networks with bursty packet delivery.
// - REBUFFER_PACKETS is usually slightly lower than START_BUFFERED_PACKETS.
// - LOW_WATER_PACKETS should be low enough to avoid unnecessary rebuffering,
//   but high enough to detect underrun risk early.
// - If playback stutters on remote networks, first try increasing
//   START_BUFFERED_PACKETS and REBUFFER_PACKETS before changing more advanced thresholds.
// - JITTER_SLOTS must be large enough to hold the working window plus reordering slack.

// ============================================================
// Timing
// ============================================================

// Timeout used to consider the downlink stream finished if no more packets arrive.
constexpr uint32_t END_TIMEOUT_MS = 1800;

// Playback task period in milliseconds.
// 20 ms matches one Opus frame at 16 kHz / 320 samples.
constexpr uint32_t PLAYBACK_PERIOD_MS = 20;

// Interval for printing runtime playback/debug statistics to Serial.
constexpr uint32_t STATS_PERIOD_MS = 2000;

// Recommendation:
// - PLAYBACK_PERIOD_MS should usually match the audio frame duration.
// - END_TIMEOUT_MS should be long enough to tolerate short network pauses,
//   but short enough to return the UI to READY state quickly after the stream ends.
// - STATS_PERIOD_MS can be lowered while debugging and raised in normal use
//   to reduce Serial noise.