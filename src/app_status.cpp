#include "app_status.h"

#include <Arduino.h>

#include "app_state.h"
#include "app_runtime_state.h"
#include "app_config.h"
#include "network_wifi.h"
#include "network_udp.h"
#include "audio_mic.h"
#include "audio_downlink.h"
#include "ui_start.h"

static unsigned long s_lastWifiConnectedAtMs = 0;
static bool s_lastWifiConnectedState = false;

static constexpr unsigned long SERVER_PING_INTERVAL_MS = 3000;
static constexpr unsigned long SERVER_PONG_TIMEOUT_MS = 6000;

static bool sendRawUdpPacket(const uint8_t *data, size_t len)
{
    return networkUdpSendRaw(serverIP.c_str(), SERVER_PORT, data, len);
}

static bool sendServerPing()
{
    static const char pingMsg[] = "PING";
    return networkUdpSendRaw(
        serverIP.c_str(),
        SERVER_PORT,
        reinterpret_cast<const uint8_t *>(pingMsg),
        4);
}

void appStatusInit()
{
    s_lastWifiConnectedState = networkWifiIsConnected();

    if (s_lastWifiConnectedState)
    {
        wifiReady = true;
        wifiStatus = WIFI_CONNECTED;
        s_lastWifiConnectedAtMs = millis();
    }
    else
    {
        wifiReady = false;
        wifiStatus = DISCONNECTED;
        s_lastWifiConnectedAtMs = 0;
    }

    lastPingTime = 0;
    lastPongTime = 0;
    udpStatus = DISCONNECTED;
}

void appStatusSetUiError(const char *line1, const char *line2)
{
    lastErrorLine1 = line1 ? line1 : "Error";
    lastErrorLine2 = line2 ? line2 : "";
}

void appStatusResetServerHeartbeat()
{
    lastPingTime = 0;
    lastPongTime = 0;
    udpStatus = DISCONNECTED;
}

bool appStatusStartNetworkServicesIfNeeded()
{
    if (!wifiReady)
    {
        Serial.println("[NET] WiFi not ready, skipping UDP/audio startup");
        return false;
    }

    if (networkServicesStarted)
    {
        return true;
    }

    Serial.println("[NET] Starting UDP/audio services...");

    if (!networkUdpInit(LOCAL_UDP_PORT))
    {
        Serial.println("[NET] networkUdpInit failed");
        return false;
    }

    audioMicSetRawUdpSender(sendRawUdpPacket);

    if (!audioDownlinkInit())
    {
        Serial.println("[NET] audioDownlinkInit failed");
        return false;
    }

    if (!audioMicInit())
    {
        Serial.println("[NET] audioMicInit failed");
        return false;
    }

    audioMicStartTask();
    audioDownlinkStartTasks();

    networkServicesStarted = true;
    appStatusResetServerHeartbeat();

    Serial.println("[NET] UDP/audio services started");
    return true;
}

void appStatusUpdateServerHeartbeat()
{
    if (!wifiReady || !networkServicesStarted || !networkWifiIsConnected())
    {
        udpStatus = DISCONNECTED;
        return;
    }

    const unsigned long now = millis();

    if (lastPingTime == 0 || (now - lastPingTime) >= SERVER_PING_INTERVAL_MS)
    {
        if (sendServerPing())
        {
            lastPingTime = now;
        }
    }

    const bool pongIsRecent =
        (lastPongTime != 0) &&
        ((now - lastPongTime) <= SERVER_PONG_TIMEOUT_MS);

    const bool pongBelongsToCurrentWifiSession =
        (lastPongTime != 0) &&
        (lastPongTime >= s_lastWifiConnectedAtMs);

    if (pongIsRecent && pongBelongsToCurrentWifiSession)
    {
        udpStatus = UDP_CONNECTED;
    }
    else
    {
        udpStatus = DISCONNECTED;
    }
}

void appStatusRefreshStartScreen()
{
    if (currentScreen != SCREEN_START)
    {
        return;
    }

    if (!wifiReady)
    {
        appStatusSetUiError("No WiFi", "Open SETUP to connect");
        uiStartApplyState(UI_ERROR);
        return;
    }

    if (udpStatus != UDP_CONNECTED)
    {
        appStatusSetUiError("No server", "Waiting backend...");
        uiStartApplyState(UI_ERROR);
        return;
    }

    if (micEnabled)
    {
        uiStartApplyState(UI_LISTENING);
        return;
    }

    if (awaitingResponse)
    {
        uiStartApplyState(UI_THINKING);
        return;
    }

    if (isPlayingResponse)
    {
        uiStartApplyState(UI_SPEAKING);
        return;
    }

    uiStartApplyState(UI_READY);
}

void appStatusRefreshConnectivity()
{
    const bool wifiConnectedNow = networkWifiIsConnected();

    if (wifiConnectedNow && !s_lastWifiConnectedState)
    {
        Serial.println("[WIFI] Connection detected");

        wifiReady = true;
        wifiStatus = WIFI_CONNECTED;

        s_lastWifiConnectedAtMs = millis();

        appStatusResetServerHeartbeat();

        if (!networkServicesStarted)
        {
            if (appStatusStartNetworkServicesIfNeeded())
            {
                Serial.println("[NET] Services started after WiFi connect");
            }
            else
            {
                Serial.println("[NET] Failed to start services after WiFi connect");
            }
        }
    }
    else if (!wifiConnectedNow && s_lastWifiConnectedState)
    {
        Serial.println("[WIFI] Connection lost");

        wifiReady = false;
        wifiStatus = DISCONNECTED;
        appStatusResetServerHeartbeat();
    }
    else if (wifiConnectedNow)
    {
        wifiReady = true;
        wifiStatus = WIFI_CONNECTED;
    }
    else
    {
        wifiReady = false;
        wifiStatus = DISCONNECTED;
        appStatusResetServerHeartbeat();
    }

    s_lastWifiConnectedState = wifiConnectedNow;

    if (currentScreen == SCREEN_START)
    {
        uiStartDrawWifiStatus();
        appStatusRefreshStartScreen();
    }
}