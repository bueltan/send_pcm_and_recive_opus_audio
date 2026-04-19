#pragma once

#include <Arduino.h>

enum UiSetupAction
{
    UI_SETUP_ACTION_NONE,
    UI_SETUP_ACTION_WIFI,
    UI_SETUP_ACTION_SERVER,
    UI_SETUP_ACTION_TOUCH,
    UI_SETUP_ACTION_BACK
};

void uiSetupInit();
void uiSetupDrawBase();
UiSetupAction uiSetupHandleTouch(uint16_t x, uint16_t y);