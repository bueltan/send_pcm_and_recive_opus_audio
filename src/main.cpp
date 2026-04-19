#include <Arduino.h>
#include <WiFi.h>

#include "display_hal.h"
#include "app_state.h"
#include "app_runtime_state.h"
#include "touch_input.h"
#include "audio_mic.h"
#include "audio_downlink.h"
#include "network_udp.h"
#include "network_wifi.h"
#include "storage_prefs.h"
#include "app_config.h"

#include "ui_start.h"
#include "ui_setup.h"
#include "ui_wifi.h"
#include "ui_wifi_password.h"

// ============================================================
// Helpers
// ============================================================
static unsigned long lastConnectivityRefreshMs = 0;
static bool lastWifiConnectedState = false;

static constexpr unsigned long SERVER_PING_INTERVAL_MS = 3000;
static constexpr unsigned long SERVER_PONG_TIMEOUT_MS = 6000;

static void setUiError(const char *line1, const char *line2)
{
    lastErrorLine1 = line1 ? line1 : "Error";
    lastErrorLine2 = line2 ? line2 : "";
}
static bool sendServerPing()
{
    static const char *pingMsg = "PING";
    return networkUdpSendRaw(serverIP.c_str(), SERVER_PORT,
                             reinterpret_cast<const uint8_t *>(pingMsg),
                             4);
}
static void updateServerHeartbeat()
{
    if (!wifiReady || !networkServicesStarted)
    {
        udpStatus = DISCONNECTED;
        return;
    }

    const unsigned long now = millis();

    if (now - lastPingTime >= SERVER_PING_INTERVAL_MS)
    {
        if (sendServerPing())
        {
            lastPingTime = now;
        }
    }

    if ((now - lastPongTime) <= SERVER_PONG_TIMEOUT_MS)
    {
        udpStatus = UDP_CONNECTED;
    }
    else
    {
        udpStatus = DISCONNECTED;
    }
}
static void refreshStartScreenStatus()
{
    if (currentScreen != SCREEN_START)
    {
        return;
    }

    if (!wifiReady)
    {
        setUiError("No WiFi", "Open SETUP to connect");
        uiStartApplyState(UI_ERROR);
        return;
    }

    if (udpStatus != UDP_CONNECTED)
    {
        setUiError("No server", "Waiting backend...");
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
static bool sendRawUdpPacket(const uint8_t *data, size_t len)
{
    return networkUdpSendRaw(serverIP.c_str(), SERVER_PORT, data, len);
}

static bool startNetworkServicesIfNeeded()
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
    Serial.println("[NET] UDP/audio services started");
    return true;
}
static void refreshConnectivityState()
{
    const bool wifiConnectedNow = networkWifiIsConnected();

    // Detect transition: disconnected -> connected
    if (wifiConnectedNow && !lastWifiConnectedState)
    {
        Serial.println("[WIFI] Connection detected after startup");

        wifiReady = true;
        wifiStatus = WIFI_CONNECTED;

        if (!networkServicesStarted)
        {
            if (startNetworkServicesIfNeeded())
            {
                Serial.println("[NET] Services started after WiFi reconnect");
            }
            else
            {
                Serial.println("[NET] Failed to start services after WiFi reconnect");
            }
        }
    }
    else if (!wifiConnectedNow && lastWifiConnectedState)
    {
        Serial.println("[WIFI] Connection lost");

        wifiReady = false;
        wifiStatus = DISCONNECTED;
        udpStatus = DISCONNECTED;
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
        udpStatus = DISCONNECTED;
    }

    lastWifiConnectedState = wifiConnectedNow;

    if (currentScreen == SCREEN_START)
    {
        uiStartDrawWifiStatus();
        refreshStartScreenStatus();
    }
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
        setUiError("No WiFi", "Open SETUP to connect");

        if (currentScreen == SCREEN_START)
        {
            uiStartApplyState(UI_ERROR);
        }
        return;
    }

    if (udpStatus != UDP_CONNECTED)
    {
        setUiError("No server", "Waiting backend...");

        if (currentScreen == SCREEN_START)
        {
            uiStartApplyState(UI_ERROR);
        }
        return;
    }

    if (!networkServicesStarted)
    {
        setUiError("Audio offline", "Network not ready");

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
    refreshStartScreenStatus();
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

static void showWifiPasswordScreen()
{
    currentScreen = SCREEN_WIFI_PASSWORD;
    uiWifiPasswordInit();
    uiWifiPasswordDrawBase();
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

    const int found = WiFi.scanNetworks(false, true);

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
// SETUP
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
    uiWifiPasswordInit();

    storagePrefsLoad();

    if (serverIP.length() == 0)
    {
        serverIP = SERVER_IP;
    }

    // Start with a neutral screen first
    showStartScreen();

    // Try WiFi using saved credentials
    if (!networkWifiConnectConfigured())
    {
        Serial.println("[WIFI] Starting without WiFi connection");
    }
    else
    {
        if (!startNetworkServicesIfNeeded())
        {
            haltWithUiError();
        }
    }

    // Refresh connectivity indicator/state after WiFi init attempt
    refreshConnectivityState();

    // Redraw start screen with the real current state
    showStartScreen();

    startTouchTask();
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
            wifiSSID = wifiNetworkNames[selectedIndex];
            wifiPASS = "";

            Serial.print("[WIFI] Selected network: ");
            Serial.println(wifiSSID);

            showWifiPasswordScreen();
        }
    }

    if (takeWifiPasswordBackRequest())
    {
        showWifiScreen();
    }

    if (takeWifiPasswordOkRequest())
    {
        Serial.print("[WIFI] Trying configured network: ");
        Serial.println(wifiSSID);

        const bool ok = networkWifiReconnect();

        if (ok)
        {
            storagePrefsSave();

            if (!startNetworkServicesIfNeeded())
            {
                Serial.println("[NET] Failed to start services after WiFi connect");
            }

            Serial.println("[WIFI] Connected and saved");
        }
        else
        {
            Serial.println("[WIFI] Failed to connect");
        }

        refreshConnectivityState();
        showStartScreen();
    }

    if (takeToggleMicRequest() && currentScreen == SCREEN_START)
    {
        handleMicToggle();
    }

    if (networkServicesStarted)
    {
        audioMicHandleLoop();
    }

    updateServerHeartbeat();

    if (millis() - lastConnectivityRefreshMs >= 1000)
    {
        lastConnectivityRefreshMs = millis();
        refreshConnectivityState();
    }

    delay(20);
}