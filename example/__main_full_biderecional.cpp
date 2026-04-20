#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>
#include <opus.h>
#include <TFT_eSPI.h>

#include "pins.h"
#include "display_hal.h"
#include "app_state.h"
#include "touch_input.h"

// ============================================================
// WiFi / UDP
// ============================================================
static const char *WIFI_SSID = "MOVISTAR-WIFI6-B700";
static const char *WIFI_PASS = "uDRsVe6jpvSfSCiQ9wPL";

static const char *SERVER_IP = "192.168.1.42";
static const uint16_t SERVER_PORT = 9999;
static const uint16_t LOCAL_UDP_PORT = 12000;

static WiFiUDP udp;

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
// Uplink PCM protocol
// ============================================================
static const uint8_t PCM_MAGIC[4] = {'P', 'C', 'M', '!'};
static const uint16_t PCM_FRAME_SAMPLES = 320;               // 40 ms @ 16 kHz
static const size_t PCM_FRAME_BYTES = PCM_FRAME_SAMPLES * 2; // PCM16 mono
static const size_t PCM_PACKET_MAX = HEADER_SIZE + PCM_FRAME_BYTES;

// ============================================================
// Jitter buffer
// ============================================================
static const int JITTER_SLOTS = 48;
static const int START_BUFFERED_PACKETS = 20;
static const int REBUFFER_PACKETS = 14;
static const int LOW_WATER_PACKETS = 6;
static const int MAX_CONSECUTIVE_MISSES = 6;
static const uint32_t MAX_AHEAD_WINDOW = 48;
static const uint32_t HARD_REBUFFER_GAP = 28;
static const uint32_t HARD_REBUFFER_MISSES = 10;

// ============================================================
// Timing
// ============================================================
static const uint32_t END_TIMEOUT_MS = 1800;
static const uint32_t PLAYBACK_PERIOD_MS = 20;
static const uint32_t STATS_PERIOD_MS = 2000;

// ============================================================
// UI
// ============================================================
static TFT_eSPI_Button btnPlay;
static TFT_eSPI_Button btnSend;

static volatile bool requestStartPlayback = false;
static volatile bool requestSendAudio = false;

static volatile bool isStreaming = false;
static volatile bool streamFinished = true;
static volatile bool wifiReady = false;

// forward declarations
static void drawStatusText(const char *line1, const char *line2 = nullptr);
static void drawPlayButton(bool inverted = false);
static void drawSendButton(bool inverted = false);

// ============================================================
// MIC / UPLINK PCM
// ============================================================
static volatile bool sendAudioEnabled = false;
static volatile bool sendAudioActive = false;

static uint32_t uplinkSequence = 0;
static uint32_t uplinkPtsSamples = 0;

static int32_t micI2SBuffer[PCM_FRAME_SAMPLES];
static int16_t micPcm16[PCM_FRAME_SAMPLES];
static uint8_t pcmUplinkPacket[PCM_PACKET_MAX];

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
SemaphoreHandle_t udpMutex = nullptr;
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

static void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

// ============================================================
// Uplink PCM
// ============================================================
static bool initI2SMic()
{
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_config.sample_rate = SAMPLE_RATE_HZ;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 8;
    i2s_config.dma_buf_len = 160;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = false;
    i2s_config.fixed_mclk = 0;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_MIC_SCK;
    pin_config.ws_io_num = I2S_MIC_WS;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num = I2S_MIC_SD;

    esp_err_t err = i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.printf("[MIC] i2s_driver_install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_MIC_PORT, &pin_config);
    if (err != ESP_OK)
    {
        Serial.printf("[MIC] i2s_set_pin failed: %d\n", err);
        return false;
    }

    err = i2s_zero_dma_buffer(I2S_MIC_PORT);
    if (err != ESP_OK)
    {
        Serial.printf("[MIC] i2s_zero_dma_buffer failed: %d\n", err);
        return false;
    }

    Serial.println("[MIC] I2S mic ready");
    return true;
}

static void resetUplinkState()
{
    uplinkSequence = 0;
    uplinkPtsSamples = 0;
}

static bool sendRawUdpPacket(const uint8_t *data, size_t len)
{
    if (udpMutex == nullptr)
        return false;

    if (xSemaphoreTake(udpMutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return false;

    udp.beginPacket(SERVER_IP, SERVER_PORT);
    udp.write(data, len);
    int ok = udp.endPacket();

    xSemaphoreGive(udpMutex);
    return ok == 1;
}

static bool sendPcmAudioPacket(
    uint32_t seq,
    uint32_t ptsSamples,
    uint16_t sampleCount,
    const uint8_t *payload,
    uint16_t payloadLen,
    uint8_t flags)
{
    if (payloadLen > PCM_FRAME_BYTES)
        return false;

    memcpy(pcmUplinkPacket + 0, PCM_MAGIC, 4);
    pcmUplinkPacket[4] = VERSION;
    pcmUplinkPacket[5] = flags;

    write_be32(pcmUplinkPacket + 6, seq);
    write_be32(pcmUplinkPacket + 10, ptsSamples);
    write_be16(pcmUplinkPacket + 14, sampleCount);
    write_be16(pcmUplinkPacket + 16, payloadLen);

    if (payloadLen > 0 && payload != nullptr)
    {
        memcpy(pcmUplinkPacket + HEADER_SIZE, payload, payloadLen);
    }

    return sendRawUdpPacket(pcmUplinkPacket, HEADER_SIZE + payloadLen);
}

static void sendPcmEndPacket()
{
    bool ok = sendPcmAudioPacket(
        uplinkSequence,
        uplinkPtsSamples,
        0,
        nullptr,
        0,
        FLAG_END);

    Serial.printf("[MIC] sent PCM END seq=%lu ok=%d\n",
                  (unsigned long)uplinkSequence,
                  ok ? 1 : 0);
}

static bool readMicFramePcm16(int16_t *outPcm, size_t sampleCount, int16_t &peakOut)
{
    size_t bytesRead = 0;
    peakOut = 0;

    esp_err_t result = i2s_read(
        I2S_MIC_PORT,
        micI2SBuffer,
        sampleCount * sizeof(int32_t),
        &bytesRead,
        portMAX_DELAY);

    if (result != ESP_OK || bytesRead == 0)
        return false;

    const int samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead != (int)sampleCount)
        return false;

    for (int i = 0; i < samplesRead; ++i)
    {
        const int32_t raw = micI2SBuffer[i];
        const int16_t sample = (int16_t)(raw >> 12);
        outPcm[i] = sample;

        const int16_t absVal = (sample < 0) ? -sample : sample;
        if (absVal > peakOut)
            peakOut = absVal;
    }

    return true;
}

static bool sendMicPcmFrame(const int16_t *pcm16, int16_t peak)
{
    const uint8_t *payload = reinterpret_cast<const uint8_t *>(pcm16);
    const uint16_t payloadLen = (uint16_t)(PCM_FRAME_SAMPLES * sizeof(int16_t));

    const bool ok = sendPcmAudioPacket(
        uplinkSequence,
        uplinkPtsSamples,
        PCM_FRAME_SAMPLES,
        payload,
        payloadLen,
        0);

    if (uplinkSequence < 8 || (uplinkSequence % 50) == 0)
    {
        Serial.printf("[MIC] sent PCM seq=%lu pts=%lu bytes=%u peak=%d ok=%d\n",
                      (unsigned long)uplinkSequence,
                      (unsigned long)uplinkPtsSamples,
                      payloadLen,
                      peak,
                      ok ? 1 : 0);
    }

    uplinkSequence++;
    uplinkPtsSamples += PCM_FRAME_SAMPLES;

    return ok;
}

static void micCaptureTask(void *parameter)
{
    (void)parameter;

    int16_t peak = 0;
    bool lastActive = false;

    for (;;)
    {
        const bool active = sendAudioEnabled;

        if (!active)
        {
            if (lastActive)
            {
                sendPcmEndPacket();
                sendAudioActive = false;
                lastActive = false;
                Serial.println("[MIC] uplink stopped");
            }

            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!lastActive)
        {
            resetUplinkState();
            sendAudioActive = true;
            lastActive = true;
            Serial.println("[MIC] uplink started");
        }

        if (!readMicFramePcm16(micPcm16, PCM_FRAME_SAMPLES, peak))
        {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        (void)sendMicPcmFrame(micPcm16, peak);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ============================================================
// Downlink jitter / decode
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

static void resetStreamState()
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
    isStreaming = false;
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

static void sendStart()
{
    resetStreamState();

    uint8_t startMsg[] = {'S', 'T', 'A', 'R', 'T'};
    (void)sendRawUdpPacket(startMsg, sizeof(startMsg));

    Serial.println("[UDP] START sent");
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
        Serial.printf("new max payload=%u\n", maxSeenPayload);
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
// UI
// ============================================================
static void drawStatusText(const char *line1, const char *line2)
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillRect(10, 70, 220, 80, TFT_BLACK);
    tft.drawRect(10, 70, 220, 80, TFT_GREEN);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(18, 85);
    tft.print(line1 ? line1 : "");

    if (line2)
    {
        tft.setTextSize(1);
        tft.setCursor(18, 120);
        tft.print(line2);
    }
    screenUnlock();
}

static void drawPlayButton(bool inverted)
{
    TFT_eSPI &tft = display();

    screenLock();
    btnPlay.initButton(
        &tft,
        70, 235,
        95, 70,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"PLAY",
        2);
    btnPlay.drawButton(inverted);
    screenUnlock();
}

static void drawSendButton(bool inverted)
{
    TFT_eSPI &tft = display();

    screenLock();
    btnSend.initButton(
        &tft,
        170, 235,
        95, 70,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)(sendAudioEnabled ? "STOP" : "SEND"),
        2);
    btnSend.drawButton(inverted);
    screenUnlock();
}

static void drawUI()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(18, 20);
    tft.print("UDP AUDIO BRIDGE");
    screenUnlock();

    drawStatusText("Ready", "PLAY=opus down | SEND=pcm up");
    drawPlayButton(false);
    drawSendButton(false);
}

static void taskTouch(void *parameter)
{
    (void)parameter;

    while (true)
    {
        uint16_t x, y;

        if (getTouchPoint(x, y))
        {
            if (btnPlay.contains(x, y))
            {
                drawPlayButton(true);
                requestStartPlayback = true;
                waitTouchRelease();
                drawPlayButton(false);
            }
            else if (btnSend.contains(x, y))
            {
                drawSendButton(true);
                requestSendAudio = true;
                waitTouchRelease();
                drawSendButton(false);
            }
            else
            {
                waitTouchRelease();
            }
        }
        else if (currentScreen == SCREEN_SERVER_EDIT)
        {
            handleServerEditScreenTouch(x, y);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
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
        int packetSize = 0;
        int n = 0;

        if (udpMutex != nullptr && xSemaphoreTake(udpMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            packetSize = udp.parsePacket();

            if (packetSize > 0)
            {
                n = udp.read(rxBuf, sizeof(rxBuf));
            }

            xSemaphoreGive(udpMutex);
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
            isStreaming = false;
            streamFinished = true;
            drawStatusText("Finished", "Tap PLAY to play again");
            continue;
        }

        if (!seenEnd && started && buffered == 0)
        {
            if ((nowMs - lastPktMs) > END_TIMEOUT_MS)
            {
                isStreaming = false;
                streamFinished = true;
                drawStatusText("Timeout", "Tap PLAY to retry");
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

                    drawStatusText("Playing", "Receiving audio...");
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
            "[STATS] rx=%lu dec=%lu fec=%lu plc=%lu miss=%lu drop=%lu dup=%lu late=%lu ahead=%lu malformed=%lu decErr=%lu hardRebuf=%lu buffered=%d expected=%lu highest=%lu end=%d rebuf=%d send=%d\n",
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
            sendAudioEnabled ? 1 : 0);

        vTaskDelay(pdMS_TO_TICKS(STATS_PERIOD_MS));
    }
}

// ============================================================
// Arduino
// ============================================================
void setup()
{
    Serial.begin(115200);
    delay(800);

    screenMutex = xSemaphoreCreateMutex();
    jitterMutex = xSemaphoreCreateMutex();
    udpMutex = xSemaphoreCreateMutex();

    initDisplayHardware();
    initTouchHardware();
    drawUI();

    drawStatusText("Connecting WiFi", "Please wait...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(300);
        Serial.print(".");
    }

    WiFi.setSleep(false);
    wifiReady = true;

    Serial.println();
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());

    if (!udp.begin(LOCAL_UDP_PORT))
    {
        drawStatusText("UDP failed", "udp.begin() error");
        while (true)
            delay(1000);
    }

    if (!i2sInit())
    {
        drawStatusText("I2S failed", "Check DAC pins");
        while (true)
            delay(1000);
    }

    if (!opusInit())
    {
        drawStatusText("Opus failed", "decoder create error");
        while (true)
            delay(1000);
    }

    if (!initI2SMic())
    {
        drawStatusText("MIC failed", "Check mic pins");
        while (true)
            delay(1000);
    }

    resetStreamState();
    streamFinished = true;
    drawStatusText("Ready", "PLAY=opus down | SEND=pcm up");

    xTaskCreatePinnedToCore(udpRxTask, "udpRxTask", 3072, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(playbackTask, "playbackTask", 8192, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskTouch, "taskTouch", 3072, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(statsTask, "statsTask", 2048, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(micCaptureTask, "micCaptureTask", 8192, nullptr, 2, nullptr, 1);
}

void loop()
{
    if (wifiReady && requestStartPlayback)
    {
        requestStartPlayback = false;
        drawStatusText("Requesting", "Sending START to server...");
        sendStart();
    }

    if (requestSendAudio)
    {
        requestSendAudio = false;
        sendAudioEnabled = !sendAudioEnabled;

        if (sendAudioEnabled)
            drawStatusText("SEND ON", "Capturing PCM + UDP");
        else
            drawStatusText("SEND OFF", "Stopping uplink...");

        drawSendButton(false);
    }

    delay(20);
}

bool takeServerEditRequest()
{
    if (!serverEditRequested)
        return false;

    serverEditRequested = false;
    return true;
}

bool takeServerEditBackRequest()
{
    if (!serverEditBackRequested)
        return false;

    serverEditBackRequested = false;
    return true;
}

bool takeServerEditOkRequest()
{
    if (!serverEditOkRequested)
        return false;

    serverEditOkRequested = false;
    return true;
}