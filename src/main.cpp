#include <Arduino.h>
#include <WiFi.h>

#include "display_hal.h"
#include "app_state.h"
#include "app_runtime_state.h"
#include "app_status.h"
#include "touch_input.h"
#include "audio_mic.h"
#include "audio_downlink.h"
#include "network_wifi.h"
#include "storage_prefs.h"
#include "app_config.h"

#include "ui_start.h"
#include "ui_setup.h"
#include "ui_wifi.h"
#include "ui_wifi_password.h"

// ============================================================
// Timing
// ============================================================
static unsigned long lastConnectivityRefreshMs = 0;
static constexpr unsigned long CONNECTIVITY_REFRESH_MS = 1000;

// ============================================================
// Fatal UI error
// ============================================================
static void haltWithUiError()
{
    if (currentScreen == SCREEN_START)
    {
        appStatusSetUiError("Fatal error", "Check serial log");
        uiStartApplyState(UI_ERROR);
    }

    while (true)
    {
        delay(1000);
    }
}

// ============================================================
// UI navigation
// ============================================================
static void showStartScreen()
{
    currentScreen = SCREEN_START;
    uiStartDrawBase();
    appStatusRefreshStartScreen();
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

// ============================================================
// WiFi scan
// ============================================================
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
    const bool wasConnectedBeforeScan = networkWifiIsConnected();
    const String previousSSID = wifiSSID;
    const String previousPASS = wifiPASS;

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

    // Scan may drop WiFi, so refresh the real state first.
    appStatusRefreshConnectivity();

    // If the device was connected before the scan, try to restore it automatically.
    if (wasConnectedBeforeScan && previousSSID.length() > 0)
    {
        Serial.println("[WIFI] Restoring previous WiFi after scan...");

        wifiSSID = previousSSID;
        wifiPASS = previousPASS;

        const bool restored = networkWifiReconnect();

        if (restored)
        {
            Serial.println("[WIFI] Previous WiFi restored");
        }
        else
        {
            Serial.println("[WIFI] Failed to restore previous WiFi");
        }

        appStatusRefreshConnectivity();
    }

    if (currentScreen == SCREEN_START)
    {
        appStatusRefreshStartScreen();
    }
}
// ============================================================
// Runtime actions
// ============================================================
static void handleMicToggle()
{
    if (!wifiReady)
    {
        appStatusSetUiError("No WiFi", "Open SETUP to connect");
        appStatusRefreshStartScreen();
        return;
    }

    if (!networkServicesStarted)
    {
        appStatusSetUiError("Audio offline", "Starting services...");
        appStatusRefreshStartScreen();
        return;
    }

    if (udpStatus != UDP_CONNECTED)
    {
        appStatusSetUiError("No server", "Waiting backend...");
        appStatusRefreshStartScreen();
        return;
    }

    if (!micEnabled && !awaitingResponse)
    {
        audioDownlinkResetStreamState();
        micEnabled = true;
        awaitingResponse = false;
        isPlayingResponse = false;
        streamFinished = false;

        appStatusRefreshStartScreen();
        return;
    }

    if (micEnabled)
    {
        micEnabled = false;
        awaitingResponse = true;
        isPlayingResponse = false;

        appStatusRefreshStartScreen();
        return;
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

    appStatusInit();

    showStartScreen();

    if (!networkWifiConnectConfigured())
    {
        Serial.println("[WIFI] Starting without WiFi connection");
    }
    else
    {
        if (!appStatusStartNetworkServicesIfNeeded())
        {
            haltWithUiError();
        }
    }

    appStatusRefreshConnectivity();
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

        String savedPass;
        if (storagePrefsLoadWifiPasswordForSsid(wifiSSID, savedPass))
        {
            wifiPASS = savedPass;
            Serial.println("[WIFI] Loaded saved password for selected SSID");
        }
        else
        {
            wifiPASS = "";
            Serial.println("[WIFI] No saved password for selected SSID");
        }

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
            storagePrefsSaveWifiCredential(wifiSSID, wifiPASS);
            storagePrefsSave();

            if (!appStatusStartNetworkServicesIfNeeded())
            {
                Serial.println("[NET] Failed to start services after WiFi connect");
            }

            appStatusResetServerHeartbeat();
            Serial.println("[WIFI] Connected and saved");
        }
        else
        {
            appStatusSetUiError("WiFi failed", "Check password");
            Serial.println("[WIFI] Failed to connect");
        }

        appStatusRefreshConnectivity();
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

    appStatusUpdateServerHeartbeat();

    if (millis() - lastConnectivityRefreshMs >= CONNECTIVITY_REFRESH_MS)
    {
        lastConnectivityRefreshMs = millis();
        appStatusRefreshConnectivity();
    }

    // Centralized UI refresh:
    // all background tasks only update shared state,
    // and the main loop is the only place that redraws START.
    if (currentScreen == SCREEN_START)
    {
        appStatusRefreshStartScreen();
    }

    delay(20);
}