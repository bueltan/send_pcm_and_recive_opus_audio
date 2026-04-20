#include "storage_prefs.h"

#include <Arduino.h>
#include <Preferences.h>

#include "app_state.h"
#include "app_config.h"

static Preferences prefs;

static constexpr int MAX_SAVED_WIFI_NETWORKS = 8;

static String makeSsidKey(int index)
{
    return "ssid_" + String(index);
}

static String makePassKey(int index)
{
    return "pass_" + String(index);
}

static int findWifiIndexBySsidNoOpen(const String &ssid)
{
    int count = prefs.getInt("wifi_count", 0);

    for (int i = 0; i < count; i++)
    {
        String savedSsid = prefs.getString(makeSsidKey(i).c_str(), "");
        if (savedSsid == ssid)
        {
            return i;
        }
    }

    return -1;
}

bool storagePrefsLoad()
{
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

    int count = prefs.getInt("wifi_count", 0);
    for (int i = 0; i < count; i++)
    {
        prefs.remove(makeSsidKey(i).c_str());
        prefs.remove(makePassKey(i).c_str());
    }
    prefs.remove("wifi_count");

    prefs.end();

    wifiSSID = "";
    wifiPASS = "";
    return true;
}

bool storagePrefsSaveWifiCredential(const String &ssid, const String &pass)
{
    if (ssid.length() == 0)
    {
        return false;
    }

    if (!prefs.begin("handwalle", false))
    {
        return false;
    }

    int count = prefs.getInt("wifi_count", 0);
    int foundIndex = -1;

    for (int i = 0; i < count; i++)
    {
        String savedSsid = prefs.getString(("ssid_" + String(i)).c_str(), "");
        if (savedSsid == ssid)
        {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex < 0)
    {
        foundIndex = count;
        prefs.putInt("wifi_count", count + 1);
    }

    prefs.putString(("ssid_" + String(foundIndex)).c_str(), ssid);
    prefs.putString(("pass_" + String(foundIndex)).c_str(), pass);

    // keep last used too
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);

    prefs.end();
    return true;
}

bool storagePrefsLoadWifiPasswordForSsid(const String &ssid, String &passOut)
{
    passOut = "";

    if (ssid.length() == 0)
    {
        return false;
    }

    if (!prefs.begin("handwalle", false))
    {
        return false;
    }

    int count = prefs.getInt("wifi_count", 0);

    for (int i = 0; i < count; i++)
    {
        String savedSsid = prefs.getString(("ssid_" + String(i)).c_str(), "");
        if (savedSsid == ssid)
        {
            passOut = prefs.getString(("pass_" + String(i)).c_str(), "");
            prefs.end();
            return true;
        }
    }

    prefs.end();
    return false;
}

bool storagePrefsRemoveWifiCredential(const String &ssid)
{
    if (ssid.length() == 0)
    {
        return false;
    }

    if (!prefs.begin("handwalle", false))
    {
        return false;
    }

    int count = prefs.getInt("wifi_count", 0);
    int idx = findWifiIndexBySsidNoOpen(ssid);

    if (idx < 0)
    {
        prefs.end();
        return false;
    }

    for (int i = idx; i < count - 1; i++)
    {
        String nextSsid = prefs.getString(makeSsidKey(i + 1).c_str(), "");
        String nextPass = prefs.getString(makePassKey(i + 1).c_str(), "");

        prefs.putString(makeSsidKey(i).c_str(), nextSsid);
        prefs.putString(makePassKey(i).c_str(), nextPass);
    }

    if (count > 0)
    {
        prefs.remove(makeSsidKey(count - 1).c_str());
        prefs.remove(makePassKey(count - 1).c_str());
        prefs.putInt("wifi_count", count - 1);
    }

    prefs.end();
    return true;
}