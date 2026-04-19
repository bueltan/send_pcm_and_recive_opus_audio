#include "network_wifi.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_state.h"
#include "app_runtime_state.h"

bool networkWifiIsConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

bool networkWifiConnectConfigured()
{
    if (wifiSSID.length() == 0)
    {
        Serial.println("[WIFI] No SSID configured");
        wifiReady = false;
        wifiStatus = DISCONNECTED;
        return false;
    }

    Serial.println("[WIFI] Connecting...");
    Serial.print("[WIFI] SSID: ");
    Serial.println(wifiSSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

    const unsigned long timeoutMs = 15000;
    const unsigned long startMs = millis();

    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < timeoutMs)
    {
        delay(300);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println();
        Serial.println("[WIFI] Connection failed");
        wifiReady = false;
        wifiStatus = DISCONNECTED;
        return false;
    }

    WiFi.setSleep(false);

    wifiReady = true;
    wifiStatus = WIFI_CONNECTED;

    Serial.println();
    Serial.print("[WIFI] Connected. IP: ");
    Serial.println(WiFi.localIP());

    return true;
}

bool networkWifiReconnect()
{
    networkWifiDisconnect();
    delay(200);
    return networkWifiConnectConfigured();
}

void networkWifiDisconnect()
{
    WiFi.disconnect(true, false);
    wifiReady = false;
    wifiStatus = DISCONNECTED;
}