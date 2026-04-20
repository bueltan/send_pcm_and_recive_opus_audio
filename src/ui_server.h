#pragma once

#include <Arduino.h>

enum UiServerAction
{
    UI_SERVER_ACTION_NONE,
    UI_SERVER_ACTION_SELECT,
    UI_SERVER_ACTION_ADD,
    UI_SERVER_ACTION_EDIT,
    UI_SERVER_ACTION_DELETE,
    UI_SERVER_ACTION_OK,
    UI_SERVER_ACTION_BACK
};

void uiServerInit();
void uiServerDrawBase();
void uiServerRedrawList();
UiServerAction uiServerHandleTouch(uint16_t x, uint16_t y, int &selectedIndex);