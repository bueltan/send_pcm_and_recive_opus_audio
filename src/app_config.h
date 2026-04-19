#pragma once

#include <Arduino.h>

// WiFi / Server
constexpr const char *WIFI_SSID = "MOVISTAR-WIFI6-B700";
constexpr const char *WIFI_PASS = "uDRsVe6jpvSfSCiQ9wPL";

constexpr const char *SERVER_IP = "74.208.158.226";
constexpr uint16_t SERVER_PORT = 9999;
constexpr uint16_t LOCAL_UDP_PORT = 12000;

// Audio common
constexpr uint32_t SAMPLE_RATE_HZ = 16000;
constexpr uint8_t AUDIO_CHANNELS = 1;

// PCM uplink
constexpr uint8_t PCM_VERSION = 1;
constexpr uint8_t PCM_FLAG_END = 0x01;

constexpr uint16_t PCM_FRAME_SAMPLES = 320;
constexpr size_t PCM_FRAME_BYTES = PCM_FRAME_SAMPLES * 2;
constexpr size_t PCM_HEADER_SIZE = 18;
constexpr size_t PCM_PACKET_MAX = PCM_HEADER_SIZE + PCM_FRAME_BYTES;

constexpr uint32_t COMMIT_DELAY_MS = 80;
constexpr uint8_t PCM_END_REPEAT_COUNT = 3;
constexpr uint32_t PCM_END_REPEAT_GAP_MS = 12;

// Opus downlink
constexpr uint8_t OPUS_VERSION = 1;
constexpr uint8_t OPUS_FLAG_END = 0x01;

constexpr uint16_t OPUS_FRAME_SAMPLES = 320;
constexpr uint16_t MAX_DECODE_SAMPLES = OPUS_FRAME_SAMPLES;
constexpr size_t MAX_OPUS_PAYLOAD = 300;
constexpr size_t OPUS_HEADER_SIZE = 18;

// Jitter buffer
constexpr int JITTER_SLOTS = 170;
constexpr int START_BUFFERED_PACKETS = 50;
constexpr int REBUFFER_PACKETS = 40;
constexpr int LOW_WATER_PACKETS = 12;
constexpr int MAX_CONSECUTIVE_MISSES = 10;
constexpr uint32_t MAX_AHEAD_WINDOW = 100;
constexpr uint32_t HARD_REBUFFER_GAP = 28;
constexpr uint32_t HARD_REBUFFER_MISSES = 10;

// Timing
constexpr uint32_t END_TIMEOUT_MS = 1800;
constexpr uint32_t PLAYBACK_PERIOD_MS = 20;
constexpr uint32_t STATS_PERIOD_MS = 2000;