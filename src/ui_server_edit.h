#pragma once

#include <Arduino.h>

enum UiServerEditAction
{
    UI_SERVER_EDIT_ACTION_NONE,
    UI_SERVER_EDIT_ACTION_BACK,
    UI_SERVER_EDIT_ACTION_OK
};

void uiServerEditInit();
void uiServerEditDrawBase();
UiServerEditAction uiServerEditHandleTouch(uint16_t x, uint16_t y);