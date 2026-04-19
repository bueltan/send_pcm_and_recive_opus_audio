#include "audio_mic.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "pins.h"
#include "app_state.h"
#include "app_runtime_state.h"
#include "app_config.h"
// ============================================================
// Audio uplink configuration
// ============================================================

static const uint8_t PCM_MAGIC[4] = {'P', 'C', 'M', '!'};


// ============================================================
// Module state
// ============================================================
static AudioMicRawUdpSender g_rawUdpSender = nullptr;

static uint32_t uplinkSequence = 0;
static uint32_t uplinkPtsSamples = 0;

static int32_t micI2SBuffer[PCM_FRAME_SAMPLES];
static int16_t micPcm16[PCM_FRAME_SAMPLES];
static uint8_t pcmUplinkPacket[PCM_PACKET_MAX];

static volatile uint32_t commitRequestAtMs = 0;

// ============================================================
// Helpers
// ============================================================
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

static bool sendRawPacket(const uint8_t *data, size_t len)
{
    if (g_rawUdpSender == nullptr)
        return false;

    return g_rawUdpSender(data, len);
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
    pcmUplinkPacket[4] = PCM_VERSION;
    pcmUplinkPacket[5] = flags;

    write_be32(pcmUplinkPacket + 6, seq);
    write_be32(pcmUplinkPacket + 10, ptsSamples);
    write_be16(pcmUplinkPacket + 14, sampleCount);
    write_be16(pcmUplinkPacket + 16, payloadLen);

    if (payloadLen > 0 && payload != nullptr)
    {
        memcpy(pcmUplinkPacket + PCM_HEADER_SIZE, payload, payloadLen);
    }

    return sendRawPacket(pcmUplinkPacket, PCM_HEADER_SIZE + payloadLen);
}

static void sendSinglePcmEndPacket()
{
    bool ok = sendPcmAudioPacket(
        uplinkSequence,
        uplinkPtsSamples,
        0,
        nullptr,
        0,
        PCM_FLAG_END);

    Serial.printf("[MIC] sent PCM END seq=%lu ok=%d\n",
                  (unsigned long)uplinkSequence,
                  ok ? 1 : 0);
}

static void sendRepeatedPcmEndPackets()
{
    for (uint8_t i = 0; i < PCM_END_REPEAT_COUNT; ++i)
    {
        sendSinglePcmEndPacket();

        if (i + 1 < PCM_END_REPEAT_COUNT)
        {
            vTaskDelay(pdMS_TO_TICKS(PCM_END_REPEAT_GAP_MS));
        }
    }
}

static void sendCommitPacket()
{
    static const uint8_t commitMsg[] = {'C', 'O', 'M', 'M', 'I', 'T'};
    bool ok = sendRawPacket(commitMsg, sizeof(commitMsg));
    Serial.printf("[MIC] sent COMMIT ok=%d\n", ok ? 1 : 0);
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
        const bool active = micEnabled;

        if (!active)
        {
            if (lastActive)
            {
                sendRepeatedPcmEndPackets();
                micStreamActive = false;
                lastActive = false;
                commitRequested = true;
                commitRequestAtMs = millis();

                Serial.printf("[MIC] uplink stopped, commit scheduled in %lu ms\n",
                              (unsigned long)COMMIT_DELAY_MS);
            }

            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!lastActive)
        {
            audioMicResetUplinkState();
            micStreamActive = true;
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
// Public API
// ============================================================
void audioMicSetRawUdpSender(AudioMicRawUdpSender sender)
{
    g_rawUdpSender = sender;
}

bool audioMicInit()
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

void audioMicResetUplinkState()
{
    uplinkSequence = 0;
    uplinkPtsSamples = 0;
}

void audioMicStartTask()
{
    xTaskCreatePinnedToCore(micCaptureTask, "micCaptureTask", 8192, nullptr, 2, nullptr, 1);
}

void audioMicHandleLoop()
{
    if (!commitRequested)
        return;

    const uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - commitRequestAtMs) >= COMMIT_DELAY_MS)
    {
        commitRequested = false;
        sendCommitPacket();
    }
}