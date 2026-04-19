#include "storage_prefs.h"

#include <Arduino.h>
#include <Preferences.h>

#include "app_state.h"
#include "app_config.h"

static Preferences prefs;

bool storagePrefsLoad()
{
    // Open read/write so the namespace is created automatically if missing.
    if (!prefs.begin("handwalle", false))
    {
        wifiSSID = "";
        wifiPASS = "";
        serverIP = SERVER_IP;
        return false;
    }

    wifiSSID = prefs.getString("wifi_ssid", "");
    wifiPASS = prefs.getString("wifi_pass", "");
    serverIP = prefs.getString("server_ip", SERVER_IP);

    prefs.end();
    return true;
}

bool storagePrefsSave()
{
    if (!prefs.begin("handwalle", false))
    {
        return false;
    }

    prefs.putString("wifi_ssid", wifiSSID);
    prefs.putString("wifi_pass", wifiPASS);
    prefs.putString("server_ip", serverIP);

    prefs.end();
    return true;
}

bool storagePrefsClearWifi()
{
    if (!prefs.begin("handwalle", false))
    {
        return false;
    }

    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");

    prefs.end();

    wifiSSID = "";
    wifiPASS = "";
    return true;
}