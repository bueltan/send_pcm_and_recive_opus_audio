#include <Arduino.h>
#include <WiFi.h>

#include "display_hal.h"
#include "app_state.h"
#include "app_runtime_state.h"
#include "ui_start.h"
#include "touch_input.h"
#include "audio_mic.h"
#include "audio_downlink.h"
#include "network_udp.h"
#include "app_config.h"

SemaphoreHandle_t udpMutex = nullptr;

// ============================================================
// Helpers
// ============================================================
static bool sendRawUdpPacket(const uint8_t *data, size_t len)
{
    return networkUdpSendRaw(SERVER_IP, SERVER_PORT, data, len);
}

static void haltWithUiError()
{
    uiStartApplyState(UI_ERROR);
    while (true)
        delay(1000);
}

static void handleMicToggle()
{
    if (!wifiReady)
    {
        uiStartApplyState(UI_ERROR);
        return;
    }

    if (!micEnabled && !awaitingResponse)
    {
        audioDownlinkResetStreamState();
        micEnabled = true;
        awaitingResponse = false;
        isPlayingResponse = false;
        uiStartApplyState(UI_LISTENING);
        return;
    }

    if (micEnabled)
    {
        micEnabled = false;
        awaitingResponse = true;
        isPlayingResponse = false;
        uiStartApplyState(UI_THINKING);
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
    udpMutex = xSemaphoreCreateMutex();

    if (screenMutex == nullptr || udpMutex == nullptr)
    {
        while (true)
            delay(1000);
    }

    initDisplayHardware();
    initTouchHardware();

    uiStartInit();
    uiStartDrawBase();
    uiStartApplyState(UI_THINKING);

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

    if (!networkUdpInit(LOCAL_UDP_PORT))
        haltWithUiError();

    audioMicSetRawUdpSender(sendRawUdpPacket);

    if (!audioDownlinkInit())
        haltWithUiError();

    if (!audioMicInit())
        haltWithUiError();

    uiStartApplyState(UI_READY);

    startTouchTask();
    audioMicStartTask();
    audioDownlinkStartTasks();
}

void loop()
{
    if (takeToggleMicRequest())
    {
        handleMicToggle();
    }

    audioMicHandleLoop();

    delay(20);
}