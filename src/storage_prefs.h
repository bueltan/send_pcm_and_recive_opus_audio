#pragma once

#include <Arduino.h>

bool storagePrefsLoad();
bool storagePrefsSave();
bool storagePrefsClearWifi();

// Multi-network WiFi credentials
bool storagePrefsSaveWifiCredential(const String &ssid, const String &pass);
bool storagePrefsLoadWifiPasswordForSsid(const String &ssid, String &passOut);
bool storagePrefsRemoveWifiCredential(const String &ssid);


bool storagePrefsLoadServerList();
bool storagePrefsSaveServerList();
bool storagePrefsAddServer(const String &ip);

bool storagePrefsRemoveServer(const String &ip);