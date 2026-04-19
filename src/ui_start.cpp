#include "ui_start.h"

#include <TFT_eSPI.h>

#include "display_hal.h"

static TFT_eSPI_Button btnMic;
static volatile UiState uiState = UI_READY;

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

void uiStartInit()
{
    uiState = UI_READY;
}

void uiStartDrawBase()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(28, 20);
    tft.print("WALL-E VOICE");
    screenUnlock();

    uiStartApplyState(UI_READY);
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
        drawStatusText("Speaking", "Receiving Opus audio...");
        break;
    case UI_ERROR:
    default:
        drawStatusText("Error", "Check serial log");
        break;
    }

    drawMicButton(false);
}

bool uiStartHandleTouch(uint16_t x, uint16_t y)
{
    if (btnMic.contains(x, y))
    {
        drawMicButton(true);
        waitTouchRelease();
        drawMicButton(false);
        return true;
    }

    return false;
}

UiState uiStartGetState()
{
    return uiState;
}