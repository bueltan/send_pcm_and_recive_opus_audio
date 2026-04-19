#include <Arduino.h>
#include <WiFi.h>

#include "display_hal.h"
#include "app_state.h"
#include "app_runtime_state.h"
#include "touch_input.h"
#include "audio_mic.h"
#include "audio_downlink.h"
#include "network_udp.h"
#include "app_config.h"

#include "ui_start.h"
#include "ui_setup.h"
#include "ui_wifi.h"

// ============================================================
// Helpers
// ============================================================
static bool sendRawUdpPacket(const uint8_t *data, size_t len)
{
    return networkUdpSendRaw(SERVER_IP, SERVER_PORT, data, len);
}

static void haltWithUiError()
{
    if (currentScreen == SCREEN_START)
    {
        uiStartApplyState(UI_ERROR);
    }

    while (true)
    {
        delay(1000);
    }
}

static void handleMicToggle()
{
    if (!wifiReady)
    {
        if (currentScreen == SCREEN_START)
        {
            uiStartApplyState(UI_ERROR);
        }
        return;
    }

    if (!micEnabled && !awaitingResponse)
    {
        audioDownlinkResetStreamState();
        micEnabled = true;
        awaitingResponse = false;
        isPlayingResponse = false;

        if (currentScreen == SCREEN_START)
        {
            uiStartApplyState(UI_LISTENING);
        }
        return;
    }

    if (micEnabled)
    {
        micEnabled = false;
        awaitingResponse = true;
        isPlayingResponse = false;

        if (currentScreen == SCREEN_START)
        {
            uiStartApplyState(UI_THINKING);
        }
    }
}

static void showStartScreen()
{
    currentScreen = SCREEN_START;
    uiStartDrawBase();

    if (!wifiReady)
    {
        uiStartApplyState(UI_ERROR);
        return;
    }

    if (micEnabled)
    {
        uiStartApplyState(UI_LISTENING);
    }
    else if (awaitingResponse)
    {
        uiStartApplyState(UI_THINKING);
    }
    else if (isPlayingResponse)
    {
        uiStartApplyState(UI_SPEAKING);
    }
    else
    {
        uiStartApplyState(UI_READY);
    }
}

static void showSetupScreen()
{
    currentScreen = SCREEN_SETUP;
    uiSetupDrawBase();
}

static void showWifiScreen()
{
    currentScreen = SCREEN_WIFI;
    uiWifiDrawBase();
}

static void clearWifiScanResults()
{
    wifiNetworkCount = 0;
    selectedWifiIndex = -1;

    for (int i = 0; i < MAX_WIFI_NETWORKS; i++)
    {
        wifiNetworkNames[i] = "";
        wifiNetworkRSSI[i] = 0;
    }
}

static void runWifiScanBlocking()
{
    wifiScanInProgress = true;
    clearWifiScanResults();

    if (currentScreen == SCREEN_WIFI)
    {
        uiWifiDrawBase();
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(100);

    int found = WiFi.scanNetworks(false, true);

    if (found > 0)
    {
        int count = found;
        if (count > MAX_WIFI_NETWORKS)
        {
            count = MAX_WIFI_NETWORKS;
        }

        for (int i = 0; i < count; i++)
        {
            wifiNetworkNames[i] = WiFi.SSID(i);
            wifiNetworkRSSI[i] = WiFi.RSSI(i);
        }

        wifiNetworkCount = count;
    }

    wifiScanInProgress = false;

    if (currentScreen == SCREEN_WIFI)
    {
        uiWifiDrawBase();
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
        {
            delay(1000);
        }
    }

    initDisplayHardware();
    initTouchHardware();

    uiStartInit();
    uiSetupInit();
    uiWifiInit();

    showStartScreen();
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
    {
        haltWithUiError();
    }

    audioMicSetRawUdpSender(sendRawUdpPacket);

    if (!audioDownlinkInit())
    {
        haltWithUiError();
    }

    if (!audioMicInit())
    {
        haltWithUiError();
    }

    showStartScreen();

    startTouchTask();
    audioMicStartTask();
    audioDownlinkStartTasks();
}

void loop()
{
    if (takeOpenSetupRequest())
    {
        showSetupScreen();
    }

    if (takeSetupBackRequest())
    {
        showStartScreen();
    }

    if (takeSetupWifiRequest())
    {
        showWifiScreen();
    }

    if (takeSetupServerRequest())
    {
        Serial.println("Setup: SERVER");
    }

    if (takeSetupTouchRequest())
    {
        Serial.println("Setup: TOUCH");
    }

    if (takeWifiBackRequest())
    {
        showSetupScreen();
    }

    if (takeWifiScanRequest())
    {
        Serial.println("[WIFI] Scan requested");
        runWifiScanBlocking();
    }

    int selectedIndex = -1;
    if (takeWifiSelectRequest(selectedIndex))
    {
        if (selectedIndex >= 0 && selectedIndex < wifiNetworkCount)
        {
            selectedWifiIndex = selectedIndex;

            Serial.print("[WIFI] Selected network: ");
            Serial.print(wifiNetworkNames[selectedIndex]);
            Serial.print(" | RSSI=");
            Serial.println(wifiNetworkRSSI[selectedIndex]);

            if (currentScreen == SCREEN_WIFI)
            {
                uiWifiDrawBase();
            }
        }
    }

    if (takeToggleMicRequest() && currentScreen == SCREEN_START)
    {
        handleMicToggle();
    }

    audioMicHandleLoop();

    delay(20);
}