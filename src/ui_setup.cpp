#include "ui_setup.h"

#include <TFT_eSPI.h>

#include "display_hal.h"

static TFT_eSPI_Button btnWifi;
static TFT_eSPI_Button btnServer;
static TFT_eSPI_Button btnTouch;
static TFT_eSPI_Button btnBack;

static void drawTitle()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(75, 20);
    tft.print("SETUP");
    screenUnlock();
}

static void drawButtons()
{
    TFT_eSPI &tft = display();

    screenLock();

    btnWifi.initButton(
        &tft,
        120, 85,
        160, 46,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"WIFI",
        2);

    btnServer.initButton(
        &tft,
        120, 145,
        160, 46,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"SERVER",
        2);

    btnTouch.initButton(
        &tft,
        120, 205,
        160, 46,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"TOUCH",
        2);

    btnBack.initButton(
        &tft,
        120, 275,
        120, 40,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"BACK",
        2);

    btnWifi.drawButton(false);
    btnServer.drawButton(false);
    btnTouch.drawButton(false);
    btnBack.drawButton(false);

    screenUnlock();
}

void uiSetupInit()
{
}

void uiSetupDrawBase()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillScreen(TFT_BLACK);
    screenUnlock();

    drawTitle();
    drawButtons();
}

UiSetupAction uiSetupHandleTouch(uint16_t x, uint16_t y)
{
    if (btnWifi.contains(x, y))
    {
        btnWifi.drawButton(true);
        waitTouchRelease();
        btnWifi.drawButton(false);
        return UI_SETUP_ACTION_WIFI;
    }

    if (btnServer.contains(x, y))
    {
        btnServer.drawButton(true);
        waitTouchRelease();
        btnServer.drawButton(false);
        return UI_SETUP_ACTION_SERVER;
    }

    if (btnTouch.contains(x, y))
    {
        btnTouch.drawButton(true);
        waitTouchRelease();
        btnTouch.drawButton(false);
        return UI_SETUP_ACTION_TOUCH;
    }

    if (btnBack.contains(x, y))
    {
        btnBack.drawButton(true);
        waitTouchRelease();
        btnBack.drawButton(false);
        return UI_SETUP_ACTION_BACK;
    }

    return UI_SETUP_ACTION_NONE;
}