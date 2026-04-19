#include "audio_downlink.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <opus.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "pins.h"
#include "network_udp.h"
#include "ui_start.h"
#include "app_runtime_state.h"

// ============================================================
// Audio / downlink Opus
// ============================================================
static const uint32_t SAMPLE_RATE_HZ = 16000;
static const uint8_t CHANNELS = 1;
static const uint16_t FRAME_SAMPLES = 320; // 20 ms @ 16 kHz
static const uint16_t MAX_DECODE_SAMPLES = FRAME_SAMPLES;
static const size_t MAX_OPUS_PAYLOAD = 300;

// ============================================================
// Downlink Opus protocol
// ============================================================
static const uint8_t OPUS_MAGIC[4] = {'O', 'P', 'U', 'S'};
static const uint8_t VERSION = 1;
static const uint8_t FLAG_END = 0x01;
static const size_t HEADER_SIZE = 18;

// ============================================================
// Jitter buffer
// ============================================================
static const int JITTER_SLOTS = 170;
static const int START_BUFFERED_PACKETS = 50;
static const int REBUFFER_PACKETS = 40;
static const int LOW_WATER_PACKETS = 12;
static const int MAX_CONSECUTIVE_MISSES = 10;
static const uint32_t MAX_AHEAD_WINDOW = 100;
static const uint32_t HARD_REBUFFER_GAP = 28;
static const uint32_t HARD_REBUFFER_MISSES = 10;

// ============================================================
// Timing
// ============================================================
static const uint32_t END_TIMEOUT_MS = 1800;
static const uint32_t PLAYBACK_PERIOD_MS = 20;
static const uint32_t STATS_PERIOD_MS = 2000;

// ============================================================
// Packet slot
// ============================================================
struct PacketSlot
{
    bool used;
    uint32_t seq;
    uint32_t pts_samples;
    uint16_t frame_samples;
    uint16_t payload_len;
    uint8_t flags;
    uint8_t payload[MAX_OPUS_PAYLOAD];
};

static PacketSlot jitter[JITTER_SLOTS];

// ============================================================
// Playback state
// ============================================================
static volatile bool playbackStarted = false;
static volatile bool rebuffering = true;
static volatile bool endSeen = false;
static volatile bool resumeLogArmed = true;

static volatile uint32_t expectedSeq = 0;
static volatile uint32_t highestSeqSeen = 0;
static volatile uint32_t endSeq = 0;
static volatile uint32_t consecutiveMisses = 0;
static volatile uint32_t lastPacketMillis = 0;

// stats
static volatile uint32_t receivedPackets = 0;
static volatile uint32_t droppedPackets = 0;
static volatile uint32_t duplicatePackets = 0;
static volatile uint32_t latePackets = 0;
static volatile uint32_t aheadDrops = 0;
static volatile uint32_t missingPackets = 0;
static volatile uint32_t decodedPackets = 0;
static volatile uint32_t decodeErrors = 0;
static volatile uint32_t malformedPackets = 0;
static volatile uint32_t plcPackets = 0;
static volatile uint32_t fecRecoveredPackets = 0;
static volatile uint32_t hardRebuffers = 0;

// ============================================================
// Decoder
// ============================================================
static OpusDecoder *opusDecoder = nullptr;
static int opusErr = 0;

static PacketSlot playbackPkt;
static int16_t pcmOut[MAX_DECODE_SAMPLES];
static int16_t silenceFrame[FRAME_SAMPLES] = {0};

// ============================================================
// RTOS / sync
// ============================================================
static SemaphoreHandle_t jitterMutex = nullptr;

// ============================================================
// Helpers
// ============================================================
static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return (uint32_t(p[0]) << 24) |
           (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) |
           uint32_t(p[3]);
}

// ============================================================
// Jitter helpers
// ============================================================
static void clearJitterNoLock()
{
    for (int i = 0; i < JITTER_SLOTS; ++i)
    {
        jitter[i].used = false;
        jitter[i].seq = 0;
        jitter[i].pts_samples = 0;
        jitter[i].frame_samples = 0;
        jitter[i].payload_len = 0;
        jitter[i].flags = 0;
    }
}

static int countBufferedPacketsNoLock()
{
    int count = 0;
    for (int i = 0; i < JITTER_SLOTS; ++i)
    {
        if (jitter[i].used)
            count++;
    }
    return count;
}

static int findSlotBySeqNoLock(uint32_t seq)
{
    for (int i = 0; i < JITTER_SLOTS; ++i)
    {
        if (jitter[i].used && jitter[i].seq == seq)
            return i;
    }
    return -1;
}

static int findFreeSlotNoLock()
{
    for (int i = 0; i < JITTER_SLOTS; ++i)
    {
        if (!jitter[i].used)
            return i;
    }
    return -1;
}

static bool getMinBufferedSeqNoLock(uint32_t &minSeq)
{
    bool found = false;
    minSeq = 0xFFFFFFFFu;

    for (int i = 0; i < JITTER_SLOTS; ++i)
    {
        if (jitter[i].used && jitter[i].seq < minSeq)
        {
            minSeq = jitter[i].seq;
            found = true;
        }
    }
    return found;
}

static void resetDecoderState()
{
    if (opusDecoder)
        opus_decoder_ctl(opusDecoder, OPUS_RESET_STATE);
}

void audioDownlinkResetStreamState()
{
    if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
    {
        clearJitterNoLock();

        playbackStarted = false;
        rebuffering = true;
        endSeen = false;
        resumeLogArmed = true;

        expectedSeq = 0;
        highestSeqSeen = 0;
        endSeq = 0;
        consecutiveMisses = 0;
        lastPacketMillis = 0;

        receivedPackets = 0;
        droppedPackets = 0;
        duplicatePackets = 0;
        latePackets = 0;
        aheadDrops = 0;
        missingPackets = 0;
        decodedPackets = 0;
        decodeErrors = 0;
        malformedPackets = 0;
        plcPackets = 0;
        fecRecoveredPackets = 0;
        hardRebuffers = 0;

        xSemaphoreGive(jitterMutex);
    }

    resetDecoderState();
    isPlayingResponse = false;
    streamFinished = false;
}

static bool popExpectedPacketLocked(PacketSlot &out)
{
    bool ok = false;

    if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
    {
        int idx = findSlotBySeqNoLock(expectedSeq);
        if (idx >= 0)
        {
            out = jitter[idx];
            jitter[idx].used = false;
            ok = true;
        }
        xSemaphoreGive(jitterMutex);
    }

    return ok;
}

static bool peekPacketBySeqLocked(uint32_t seq, PacketSlot &out)
{
    bool ok = false;

    if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
    {
        int idx = findSlotBySeqNoLock(seq);
        if (idx >= 0)
        {
            out = jitter[idx];
            ok = true;
        }
        xSemaphoreGive(jitterMutex);
    }

    return ok;
}

static bool insertPacketLocked(uint32_t seq,
                               uint32_t pts_samples,
                               uint16_t frame_samples,
                               uint16_t payload_len,
                               uint8_t flags,
                               const uint8_t *payload)
{
    bool inserted = false;

    if (payload_len > MAX_OPUS_PAYLOAD)
    {
        malformedPackets++;
        droppedPackets++;
        return false;
    }

    if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
    {
        if (findSlotBySeqNoLock(seq) >= 0)
        {
            duplicatePackets++;
        }
        else if (playbackStarted && !rebuffering && seq < expectedSeq)
        {
            latePackets++;
        }
        else if (playbackStarted && !rebuffering && seq > expectedSeq + MAX_AHEAD_WINDOW)
        {
            aheadDrops++;
            droppedPackets++;
        }
        else
        {
            int idx = findFreeSlotNoLock();
            if (idx < 0)
            {
                droppedPackets++;
                xSemaphoreGive(jitterMutex);
                return false;
            }

            jitter[idx].used = true;
            jitter[idx].seq = seq;
            jitter[idx].pts_samples = pts_samples;
            jitter[idx].frame_samples = frame_samples;
            jitter[idx].payload_len = payload_len;
            jitter[idx].flags = flags;
            memcpy(jitter[idx].payload, payload, payload_len);

            if (receivedPackets == 0 || seq > highestSeqSeen)
                highestSeqSeen = seq;

            receivedPackets++;
            lastPacketMillis = millis();
            inserted = true;
        }

        xSemaphoreGive(jitterMutex);
    }

    return inserted;
}

static void markEndPacket(uint32_t seq)
{
    if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
    {
        endSeen = true;
        endSeq = seq;
        xSemaphoreGive(jitterMutex);
    }
}

// ============================================================
// Playback init
// ============================================================
static bool i2sInit()
{
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = SAMPLE_RATE_HZ;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_DAC_BCK;
    pins.ws_io_num = I2S_DAC_WS;
    pins.data_out_num = I2S_DAC_DATA;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(I2S_DAC_PORT, &config, 0, nullptr);
    if (err != ESP_OK)
    {
        Serial.printf("[I2S] driver_install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_DAC_PORT, &pins);
    if (err != ESP_OK)
    {
        Serial.printf("[I2S] set_pin failed: %d\n", err);
        return false;
    }

    err = i2s_zero_dma_buffer(I2S_DAC_PORT);
    if (err != ESP_OK)
    {
        Serial.printf("[I2S] zero_dma_buffer failed: %d\n", err);
        return false;
    }

    return true;
}

static bool opusInit()
{
    opusDecoder = opus_decoder_create(SAMPLE_RATE_HZ, CHANNELS, &opusErr);
    if (!opusDecoder || opusErr != OPUS_OK)
    {
        Serial.printf("[OPUS] decoder create failed: %d\n", opusErr);
        return false;
    }

    resetDecoderState();
    return true;
}

static void writePcmToI2S(const int16_t *pcm, size_t samples)
{
    size_t bytesWritten = 0;
    i2s_write(I2S_DAC_PORT, pcm, samples * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
}

static void writeSilenceFrame()
{
    writePcmToI2S(silenceFrame, FRAME_SAMPLES);
}

static void decodeAndWriteLossRecovery()
{
    PacketSlot nextPkt;

    if (peekPacketBySeqLocked(expectedSeq + 1, nextPkt))
    {
        int fecSamples = opus_decode(
            opusDecoder,
            nextPkt.payload,
            nextPkt.payload_len,
            pcmOut,
            FRAME_SAMPLES,
            1);

        if (fecSamples > 0)
        {
            writePcmToI2S(pcmOut, fecSamples);
            fecRecoveredPackets++;
            return;
        }
    }

    int plcSamples = opus_decode(
        opusDecoder,
        nullptr,
        0,
        pcmOut,
        FRAME_SAMPLES,
        0);

    if (plcSamples > 0)
    {
        writePcmToI2S(pcmOut, plcSamples);
        plcPackets++;
    }
    else
    {
        writeSilenceFrame();
        decodeErrors++;
    }
}

static bool parseAndQueuePacket(const uint8_t *buf, size_t len)
{
    if (len < HEADER_SIZE)
        return false;

    if (memcmp(buf, OPUS_MAGIC, 4) != 0)
        return false;

    uint8_t version = buf[4];
    uint8_t flags = buf[5];
    uint32_t seq = read_be32(buf + 6);
    uint32_t pts_samples = read_be32(buf + 10);
    uint16_t frame_samples = read_be16(buf + 14);
    uint16_t payload_len = read_be16(buf + 16);

    static uint16_t maxSeenPayload = 0;
    if (payload_len > maxSeenPayload)
    {
        maxSeenPayload = payload_len;
        Serial.printf("[UDP] new max opus payload=%u\n", maxSeenPayload);
    }

    if (version != VERSION)
        return false;

    if ((HEADER_SIZE + payload_len) != len)
        return false;

    if ((flags & FLAG_END) != 0)
    {
        markEndPacket(seq);
        Serial.printf("[UDP] END packet seq=%lu\n", (unsigned long)seq);
        return true;
    }

    if (frame_samples != FRAME_SAMPLES)
        return false;

    return insertPacketLocked(seq, pts_samples, frame_samples, payload_len, flags, buf + HEADER_SIZE);
}

// ============================================================
// Tasks
// ============================================================
static void udpRxTask(void *parameter)
{
    (void)parameter;
    static uint8_t rxBuf[HEADER_SIZE + MAX_OPUS_PAYLOAD + 16];

    while (true)
    {
        int packetSize = networkUdpParsePacket();
        int n = 0;

        if (packetSize > 0)
        {
            n = networkUdpRead(rxBuf, sizeof(rxBuf));
        }

        if (packetSize > 0 && n > 0)
        {
            bool ok = parseAndQueuePacket(rxBuf, (size_t)n);
            if (!ok)
                malformedPackets++;
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

static void playbackTask(void *parameter)
{
    (void)parameter;
    TickType_t lastWake = xTaskGetTickCount();

    while (true)
    {
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(PLAYBACK_PERIOD_MS));

        bool started, rebuf, seenEnd;
        uint32_t expSeq, hiSeq, eSeq, lastPktMs, consecMiss;
        int buffered = 0;
        uint32_t minSeq = 0;
        bool foundMin = false;
        uint32_t nowMs = millis();

        if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
        {
            started = playbackStarted;
            rebuf = rebuffering;
            seenEnd = endSeen;
            expSeq = expectedSeq;
            hiSeq = highestSeqSeen;
            eSeq = endSeq;
            lastPktMs = lastPacketMillis;
            consecMiss = consecutiveMisses;
            buffered = countBufferedPacketsNoLock();
            foundMin = (buffered > 0) ? getMinBufferedSeqNoLock(minSeq) : false;
            xSemaphoreGive(jitterMutex);
        }
        else
        {
            continue;
        }

        if (seenEnd && buffered == 0 && expSeq >= eSeq)
        {
            isPlayingResponse = false;
            awaitingResponse = false;
            streamFinished = true;
            uiStartApplyState(UI_READY);
            continue;
        }

        if (!seenEnd && started && buffered == 0)
        {
            if ((nowMs - lastPktMs) > END_TIMEOUT_MS)
            {
                isPlayingResponse = false;
                awaitingResponse = false;
                streamFinished = true;
                uiStartApplyState(UI_READY);
                continue;
            }
        }

        if (rebuf || !started)
        {
            bool canResume = false;
            int needed = started ? REBUFFER_PACKETS : START_BUFFERED_PACKETS;

            if (seenEnd)
                canResume = (buffered > 0 && foundMin);
            else
                canResume = (buffered >= needed && foundMin);

            if (canResume)
            {
                if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
                {
                    expectedSeq = minSeq;
                    playbackStarted = true;
                    rebuffering = false;
                    consecutiveMisses = 0;
                    bool shouldLog = resumeLogArmed;
                    resumeLogArmed = false;
                    xSemaphoreGive(jitterMutex);

                    if (shouldLog)
                    {
                        Serial.printf("[PLAYBACK] start/resume at seq=%lu buffered=%d\n",
                                      (unsigned long)minSeq, buffered);
                    }

                    if (!isPlayingResponse)
                    {
                        isPlayingResponse = true;
                        awaitingResponse = false;
                        uiStartApplyState(UI_SPEAKING);
                    }
                }
            }
            else
            {
                continue;
            }
        }

        if (!popExpectedPacketLocked(playbackPkt))
        {
            bool bufferIsActuallyLow = (buffered <= LOW_WATER_PACKETS);
            bool tooManyMissesWithLowBuffer = ((consecMiss + 1) >= MAX_CONSECUTIVE_MISSES) &&
                                              (buffered <= (REBUFFER_PACKETS / 2));
            bool seqGapTooLarge = (hiSeq > expSeq) && ((hiSeq - expSeq) > HARD_REBUFFER_GAP);
            bool shouldHardRebuffer = seqGapTooLarge && ((consecMiss + 1) >= HARD_REBUFFER_MISSES);

            if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
            {
                missingPackets++;
                consecutiveMisses++;

                if (bufferIsActuallyLow || tooManyMissesWithLowBuffer || shouldHardRebuffer)
                {
                    rebuffering = true;
                    resumeLogArmed = true;
                    if (shouldHardRebuffer)
                        hardRebuffers++;
                }

                xSemaphoreGive(jitterMutex);
            }

            if (bufferIsActuallyLow || tooManyMissesWithLowBuffer || shouldHardRebuffer)
                continue;

            decodeAndWriteLossRecovery();

            if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
            {
                expectedSeq++;
                xSemaphoreGive(jitterMutex);
            }
            continue;
        }

        if (!isPlayingResponse)
        {
            isPlayingResponse = true;
            awaitingResponse = false;
            uiStartApplyState(UI_SPEAKING);
        }

        if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
        {
            consecutiveMisses = 0;
            xSemaphoreGive(jitterMutex);
        }

        int decodedSamples = opus_decode(
            opusDecoder,
            playbackPkt.payload,
            playbackPkt.payload_len,
            pcmOut,
            FRAME_SAMPLES,
            0);

        if (decodedSamples < 0)
        {
            decodeErrors++;
            decodeAndWriteLossRecovery();
        }
        else
        {
            writePcmToI2S(pcmOut, decodedSamples);
            decodedPackets++;
        }

        if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
        {
            expectedSeq++;
            xSemaphoreGive(jitterMutex);
        }
    }
}

static void statsTask(void *parameter)
{
    (void)parameter;

    while (true)
    {
        int buffered = 0;
        uint32_t expSeq = 0;
        uint32_t hiSeq = 0;
        bool seenEnd = false;
        bool rebuf = false;

        if (xSemaphoreTake(jitterMutex, portMAX_DELAY) == pdTRUE)
        {
            buffered = countBufferedPacketsNoLock();
            expSeq = expectedSeq;
            hiSeq = highestSeqSeen;
            seenEnd = endSeen;
            rebuf = rebuffering;
            xSemaphoreGive(jitterMutex);
        }

        Serial.printf(
            "[STATS] rx=%lu dec=%lu fec=%lu plc=%lu miss=%lu drop=%lu dup=%lu late=%lu ahead=%lu malformed=%lu decErr=%lu hardRebuf=%lu buffered=%d expected=%lu highest=%lu end=%d rebuf=%d mic=%d think=%d speak=%d\n",
            (unsigned long)receivedPackets,
            (unsigned long)decodedPackets,
            (unsigned long)fecRecoveredPackets,
            (unsigned long)plcPackets,
            (unsigned long)missingPackets,
            (unsigned long)droppedPackets,
            (unsigned long)duplicatePackets,
            (unsigned long)latePackets,
            (unsigned long)aheadDrops,
            (unsigned long)malformedPackets,
            (unsigned long)decodeErrors,
            (unsigned long)hardRebuffers,
            buffered,
            (unsigned long)expSeq,
            (unsigned long)hiSeq,
            seenEnd ? 1 : 0,
            rebuf ? 1 : 0,
            micEnabled ? 1 : 0,
            awaitingResponse ? 1 : 0,
            isPlayingResponse ? 1 : 0);

        vTaskDelay(pdMS_TO_TICKS(STATS_PERIOD_MS));
    }
}

// ============================================================
// Public API
// ============================================================
bool audioDownlinkInit()
{
    jitterMutex = xSemaphoreCreateMutex();
    if (jitterMutex == nullptr)
    {
        Serial.println("[DOWNLINK] Failed to create jitterMutex");
        return false;
    }

    if (!i2sInit())
        return false;

    if (!opusInit())
        return false;

    audioDownlinkResetStreamState();
    streamFinished = true;
    return true;
}

void audioDownlinkStartTasks()
{
    xTaskCreatePinnedToCore(udpRxTask, "udpRxTask", 3072, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(playbackTask, "playbackTask", 8192, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(statsTask, "statsTask", 2048, nullptr, 1, nullptr, 1);
}