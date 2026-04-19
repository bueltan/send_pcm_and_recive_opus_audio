#pragma once

#include <Arduino.h>

enum UiWifiAction
{
    UI_WIFI_ACTION_NONE,
    UI_WIFI_ACTION_SCAN,
    UI_WIFI_ACTION_BACK,
    UI_WIFI_ACTION_SELECT_NETWORK
};

void uiWifiInit();
void uiWifiDrawBase();
void uiWifiRedrawList();
UiWifiAction uiWifiHandleTouch(uint16_t x, uint16_t y, int &selectedIndex);