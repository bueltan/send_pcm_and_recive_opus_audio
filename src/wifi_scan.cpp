#include "wifi_scan.h"

#include <WiFi.h>

#include "app_state.h"

void wifiScanStart()
{
    wifiScanInProgress = true;
    wifiNetworkCount = 0;
    selectedWifiIndex = -1;
}

void wifiScanRunBlocking()
{
    wifiScanStart();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int found = WiFi.scanNetworks(false, true);

    wifiNetworkCount = 0;

    if (found > 0)
    {
        int count = found;
        if (count > MAX_WIFI_NETWORKS)
            count = MAX_WIFI_NETWORKS;

        for (int i = 0; i < count; i++)
        {
            wifiNetworkNames[i] = WiFi.SSID(i);
            wifiNetworkRSSI[i] = WiFi.RSSI(i);
        }

        wifiNetworkCount = count;
    }

    wifiScanInProgress = false;
}