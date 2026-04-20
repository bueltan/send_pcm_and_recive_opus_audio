#include "ui_server_edit.h"

#include <TFT_eSPI.h>

#include "app_state.h"
#include "display_hal.h"
#include "ui_keyboard.h"

static UiKeyboardState keyboardState;
static bool keyboardReady = false;

static constexpr int BACK_X = 8;
static constexpr int BACK_Y = 8;
static constexpr int BACK_W = 28;
static constexpr int BACK_H = 28;

static const uint8_t backArrow16x16[] = {
    0b00000000, 0b00000000,
    0b00000001, 0b00000000,
    0b00000011, 0b00000000,
    0b00000111, 0b00000000,
    0b00001111, 0b11111110,
    0b00011111, 0b11111110,
    0b00111111, 0b11111110,
    0b01111111, 0b11111110,
    0b00111111, 0b11111110,
    0b00011111, 0b11111110,
    0b00001111, 0b11111110,
    0b00000111, 0b00000000,
    0b00000011, 0b00000000,
    0b00000001, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000};

static void drawTitle()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(54, 10);
    tft.print("SERVER IP");

    tft.drawRoundRect(BACK_X, BACK_Y, BACK_W, BACK_H, 4, TFT_GREEN);
    tft.drawBitmap(BACK_X + 6, BACK_Y + 6, backArrow16x16, 16, 16, TFT_GREEN);
    screenUnlock();
}

static void drawHeader()
{
    TFT_eSPI &tft = display();

    String shown = serverIP;
    if (shown.length() > 24)
    {
        shown = shown.substring(shown.length() - 24);
    }

    screenLock();
    tft.fillRect(0, 34, 240, 62, TFT_BLACK);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 38);
    tft.print("Server:");

    tft.drawRect(10, 56, 220, 24, TFT_GREEN);
    tft.setCursor(14, 64);
    tft.print(shown);
    screenUnlock();
}

static bool isBackTouched(uint16_t x, uint16_t y)
{
    return x >= BACK_X &&
           x < (BACK_X + BACK_W) &&
           y >= BACK_Y &&
           y < (BACK_Y + BACK_H);
}

void uiServerEditInit()
{
    keyboardState.text = &serverIP;
    keyboardState.capsLock = false;
    keyboardState.symbolMode = false;
    keyboardState.maxLength = 64;

    uiKeyboardInit(&keyboardState);
    keyboardReady = true;
}

void uiServerEditDrawBase()
{
    TFT_eSPI &tft = display();

    if (!keyboardReady)
    {
        uiServerEditInit();
    }

    screenLock();
    tft.fillScreen(TFT_BLACK);
    screenUnlock();

    drawTitle();
    drawHeader();
    uiKeyboardDraw();
}

UiServerEditAction uiServerEditHandleTouch(uint16_t x, uint16_t y)
{
    if (isBackTouched(x, y))
    {
        waitTouchRelease();
        return UI_SERVER_EDIT_ACTION_BACK;
    }

    UiKeyboardAction action = uiKeyboardHandleTouch(x, y);

    if (action == UI_KEYBOARD_ACTION_CHANGED)
    {
        drawHeader();
        uiKeyboardDraw();
        return UI_SERVER_EDIT_ACTION_NONE;
    }

    if (action == UI_KEYBOARD_ACTION_OK)
    {
        return UI_SERVER_EDIT_ACTION_OK;
    }

    return UI_SERVER_EDIT_ACTION_NONE;
}