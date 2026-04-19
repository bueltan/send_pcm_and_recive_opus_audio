#pragma once

#include <Arduino.h>

enum UiState : uint8_t
{
    UI_READY = 0,
    UI_LISTENING,
    UI_THINKING,
    UI_SPEAKING,
    UI_ERROR,
};

void uiStartInit();
void uiStartDrawBase();
void uiStartApplyState(UiState state);
bool uiStartHandleTouch(uint16_t x, uint16_t y);
UiState uiStartGetState();