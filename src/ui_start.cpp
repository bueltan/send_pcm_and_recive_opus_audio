#include "ui_start.h"

#include <TFT_eSPI.h>

#include "display_hal.h"

static TFT_eSPI_Button btnMic;
static volatile UiState uiState = UI_READY;

// Setup button bounds
static constexpr int SETUP_X = 188;
static constexpr int SETUP_Y = 14;
static constexpr int SETUP_W = 36;
static constexpr int SETUP_H = 36;

// Simple 16x16 monochrome setup icon
static const uint8_t setupIcon16x16[] = {
    0b00000111, 0b11100000,
    0b00001100, 0b00110000,
    0b00011000, 0b00011000,
    0b00110011, 0b11001100,
    0b00100111, 0b11100100,
    0b01100110, 0b01100110,
    0b01101100, 0b00110110,
    0b11111100, 0b00111111,
    0b11111100, 0b00111111,
    0b01101100, 0b00110110,
    0b01100110, 0b01100110,
    0b00100111, 0b11100100,
    0b00110011, 0b11001100,
    0b00011000, 0b00011000,
    0b00001100, 0b00110000,
    0b00000111, 0b11100000};

static const char *uiStateToButtonLabel(UiState state)
{
    switch (state)
    {
    case UI_LISTENING:
        return "STOP";
    case UI_THINKING:
        return "WAIT";
    case UI_SPEAKING:
        return "MIC";
    case UI_ERROR:
        return "ERR";
    case UI_READY:
    default:
        return "MIC";
    }
}

static void drawTitle()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(22, 22);
    tft.print("HAND-WALL-E");
    screenUnlock();
}

static void drawStatusText(const char *line1, const char *line2 = nullptr)
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillRect(10, 70, 220, 90, TFT_BLACK);
    tft.drawRect(10, 70, 220, 90, TFT_GREEN);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(18, 88);
    tft.print(line1 ? line1 : "");

    if (line2)
    {
        tft.setTextSize(1);
        tft.setCursor(18, 126);
        tft.print(line2);
    }
    screenUnlock();
}

static void drawMicButton(bool inverted = false)
{
    TFT_eSPI &tft = display();

    screenLock();
    btnMic.initButton(
        &tft,
        120, 235,
        140, 78,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)uiStateToButtonLabel(uiState),
        2);
    btnMic.drawButton(inverted);
    screenUnlock();
}

static void drawSetupButton(bool pressed = false)
{
    TFT_eSPI &tft = display();

    const uint16_t bg = pressed ? TFT_GREEN : TFT_BLACK;
    const uint16_t fg = pressed ? TFT_BLACK : TFT_GREEN;

    screenLock();
    tft.fillRoundRect(SETUP_X, SETUP_Y, SETUP_W, SETUP_H, 4, bg);
    tft.drawRoundRect(SETUP_X, SETUP_Y, SETUP_W, SETUP_H, 4, TFT_GREEN);

    const int iconX = SETUP_X + (SETUP_W - 16) / 2;
    const int iconY = SETUP_Y + (SETUP_H - 16) / 2;
    tft.drawBitmap(iconX, iconY, setupIcon16x16, 16, 16, fg);
    screenUnlock();
}

static bool isSetupTouched(uint16_t x, uint16_t y)
{
    return x >= SETUP_X && x < (SETUP_X + SETUP_W) &&
           y >= SETUP_Y && y < (SETUP_Y + SETUP_H);
}

void uiStartInit()
{
    uiState = UI_READY;
}

void uiStartDrawBase()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillScreen(TFT_BLACK);
    screenUnlock();

    drawTitle();
    drawSetupButton(false);

    // Importante: como uiState ya arranca en UI_READY,
    // llamamos primero a drawStatusText y drawMicButton
    // para evitar que uiStartApplyState(UI_READY) salga por early return.
    drawStatusText("Ready", "Tap MIC to talk");
    drawMicButton(false);
}

void uiStartApplyState(UiState state)
{
    if (state == uiState)
        return;

    uiState = state;

    switch (state)
    {
    case UI_READY:
        drawStatusText("Ready", "Tap MIC to talk");
        break;
    case UI_LISTENING:
        drawStatusText("Listening", "Tap MIC again to send");
        break;
    case UI_THINKING:
        drawStatusText("Thinking", "Waiting response...");
        break;
    case UI_SPEAKING:
        drawStatusText("Speaking", "Receiving audio...");
        break;
    case UI_ERROR:
    default:
        drawStatusText("Error", "Check serial log");
        break;
    }

    drawMicButton(false);
    drawSetupButton(false);
}

UiStartAction uiStartHandleTouch(uint16_t x, uint16_t y)
{
    if (isSetupTouched(x, y))
    {
        drawSetupButton(true);
        waitTouchRelease();
        drawSetupButton(false);
        return UI_ACTION_SETUP;
    }

    if (btnMic.contains(x, y))
    {
        drawMicButton(true);
        waitTouchRelease();
        drawMicButton(false);
        return UI_ACTION_MIC;
    }

    return UI_ACTION_NONE;
}

UiState uiStartGetState()
{
    return uiState;
}