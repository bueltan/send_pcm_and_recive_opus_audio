#pragma once

#include <Arduino.h>

enum UiState
{
    UI_READY,
    UI_LISTENING,
    UI_THINKING,
    UI_SPEAKING,
    UI_ERROR
};

enum UiStartAction
{
    UI_ACTION_NONE,
    UI_ACTION_MIC,
    UI_ACTION_SETUP
};

void uiStartInit();
void uiStartDrawBase();
void uiStartApplyState(UiState state);
UiStartAction uiStartHandleTouch(uint16_t x, uint16_t y);
UiState uiStartGetState();