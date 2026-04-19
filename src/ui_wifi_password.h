#pragma once

#include <Arduino.h>

enum UiWifiPasswordAction
{
    UI_WIFI_PASSWORD_ACTION_NONE,
    UI_WIFI_PASSWORD_ACTION_BACK,
    UI_WIFI_PASSWORD_ACTION_OK
};

void uiWifiPasswordInit();
void uiWifiPasswordDrawBase();
UiWifiPasswordAction uiWifiPasswordHandleTouch(uint16_t x, uint16_t y);