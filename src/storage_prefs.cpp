#include "storage_prefs.h"

#include <Arduino.h>
#include <Preferences.h>

#include "app_state.h"
#include "app_config.h"

static Preferences prefs;

static constexpr int MAX_SAVED_WIFI_NETWORKS = 8;


static constexpr int MAX_SAVED_SERVERS = MAX_SERVER_ENTRIES;

static String makeServerKey(int index)
{
    return "server_" + String(index);
}

static String makeSsidKey(int index)
{
    return "ssid_" + String(index);
}

static String makePassKey(int index)
{
    return "pass_" + String(index);
}


bool storagePrefsRemoveServer(const String &ip)
{
    if (ip.length() == 0)
    {
        return false;
    }

    int idx = -1;
    for (int i = 0; i < serverListCount; i++)
    {
        if (serverList[i] == ip)
        {
            idx = i;
            break;
        }
    }

    if (idx < 0)
    {
        return false;
    }

    for (int i = idx; i < serverListCount - 1; i++)
    {
        serverList[i] = serverList[i + 1];
    }

    if (serverListCount > 0)
    {
        serverListCount--;
        serverList[serverListCount] = "";
    }

    // Never leave the list empty.
    if (serverListCount == 0)
    {
        serverList[0] = SERVER_IP;
        serverListCount = 1;
        selectedServerIndex = 0;
    }
    else
    {
        if (selectedServerIndex >= serverListCount)
        {
            selectedServerIndex = serverListCount - 1;
        }
        if (selectedServerIndex < 0)
        {
            selectedServerIndex = 0;
        }
    }

    // If active server was removed, fall back to selected/default.
    bool activeStillExists = false;
    for (int i = 0; i < serverListCount; i++)
    {
        if (serverList[i] == serverIP)
        {
            activeStillExists = true;
            break;
        }
    }

    if (!activeStillExists)
    {
        serverIP = serverList[selectedServerIndex];
    }

    return storagePrefsSaveServerList() && storagePrefsSave();
}
bool storagePrefsLoadServerList()
{
    serverListCount = 0;
    selectedServerIndex = -1;

    if (!prefs.begin("handwalle", false))
    {
        serverListCount = 1;
        serverList[0] = SERVER_IP;
        selectedServerIndex = 0;
        return false;
    }

    int count = prefs.getInt("server_count", 0);
    if (count > MAX_SAVED_SERVERS)
        count = MAX_SAVED_SERVERS;

    for (int i = 0; i < count; i++)
    {
        String value = prefs.getString(makeServerKey(i).c_str(), "");
        if (value.length() > 0)
        {
            serverList[serverListCount++] = value;
        }
    }

    bool hasDefault = false;
    for (int i = 0; i < serverListCount; i++)
    {
        if (serverList[i] == SERVER_IP)
        {
            hasDefault = true;
            break;
        }
    }

    if (!hasDefault && serverListCount < MAX_SERVER_ENTRIES)
    {
        serverList[serverListCount++] = SERVER_IP;
    }

    if (serverListCount == 0)
    {
        serverList[0] = SERVER_IP;
        serverListCount = 1;
    }

    selectedServerIndex = 0;
    for (int i = 0; i < serverListCount; i++)
    {
        if (serverList[i] == serverIP)
        {
            selectedServerIndex = i;
            break;
        }
    }

    prefs.end();
    return true;
}

bool storagePrefsSaveServerList()
{
    if (!prefs.begin("handwalle", false))
    {
        return false;
    }

    prefs.putInt("server_count", serverListCount);

    for (int i = 0; i < MAX_SERVER_ENTRIES; i++)
    {
        String key = makeServerKey(i);

        if (i < serverListCount)
        {
            prefs.putString(key.c_str(), serverList[i]);
        }
        else
        {
            prefs.remove(key.c_str());
        }
    }

    prefs.end();
    return true;
}

bool storagePrefsAddServer(const String &ip)
{
    if (ip.length() == 0)
    {
        return false;
    }

    if (serverListCount <= 0)
    {
        serverList[0] = SERVER_IP;
        serverListCount = 1;
    }

    for (int i = 0; i < serverListCount; i++)
    {
        if (serverList[i] == ip)
        {
            selectedServerIndex = i;
            return true;
        }
    }

    if (serverListCount >= MAX_SERVER_ENTRIES)
    {
        return false;
    }

    serverList[serverListCount] = ip;
    selectedServerIndex = serverListCount;
    serverListCount++;

    return storagePrefsSaveServerList();
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