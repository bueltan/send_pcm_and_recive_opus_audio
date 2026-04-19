#pragma once

#include <Arduino.h>

enum UiKeyboardAction
{
    UI_KEYBOARD_ACTION_NONE,
    UI_KEYBOARD_ACTION_CHANGED,
    UI_KEYBOARD_ACTION_OK
};

struct UiKeyboardState
{
    String *text;
    bool capsLock;
    bool symbolMode;
    size_t maxLength;
};

void uiKeyboardInit(UiKeyboardState *state);
void uiKeyboardDraw();
UiKeyboardAction uiKeyboardHandleTouch(uint16_t x, uint16_t y);