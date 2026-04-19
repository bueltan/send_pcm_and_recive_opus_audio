#include "ui_wifi.h"

#include <TFT_eSPI.h>

#include "app_state.h"
#include "display_hal.h"

static TFT_eSPI_Button btnScan;
static TFT_eSPI_Button btnBack;

static constexpr int LIST_X = 10;
static constexpr int LIST_Y = 52;
static constexpr int LIST_W = 220;
static constexpr int ROW_H = 24;
static constexpr int MAX_VISIBLE_ROWS = 7;

static void drawTitle()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(84, 16);
    tft.print("WIFI");
    screenUnlock();
}

static void drawStatus()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillRect(10, 36, 220, 14, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 38);

    if (wifiScanInProgress)
    {
        tft.print("Scanning...");
    }
    else
    {
        tft.print("Found: ");
        tft.print(wifiNetworkCount);
    }

    screenUnlock();
}

static void drawButtons()
{
    TFT_eSPI &tft = display();

    screenLock();

    btnScan.initButton(
        &tft,
        65, 295,
        100, 34,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"SCAN",
        2);

    btnBack.initButton(
        &tft,
        175, 295,
        100, 34,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"BACK",
        2);

    btnScan.drawButton(false);
    btnBack.drawButton(false);

    screenUnlock();
}

static void drawNetworkRow(int row, const String &name, int rssi, bool selected)
{
    TFT_eSPI &tft = display();

    const int y = LIST_Y + row * ROW_H;
    const uint16_t bg = selected ? TFT_GREEN : TFT_BLACK;
    const uint16_t fg = selected ? TFT_BLACK : TFT_GREEN;

    screenLock();

    tft.fillRect(LIST_X, y, LIST_W, ROW_H - 2, bg);
    tft.drawRect(LIST_X, y, LIST_W, ROW_H - 2, TFT_GREEN);

    tft.setTextColor(fg, bg);
    tft.setTextSize(1);

    tft.setCursor(LIST_X + 6, y + 6);

    String shownName = name;
    if (shownName.length() > 20)
    {
        shownName = shownName.substring(0, 20);
    }
    tft.print(shownName);

    tft.setCursor(LIST_X + 180, y + 6);
    tft.print(rssi);

    screenUnlock();
}

void uiWifiInit()
{
}

void uiWifiRedrawList()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillRect(LIST_X, LIST_Y, LIST_W, ROW_H * MAX_VISIBLE_ROWS, TFT_BLACK);
    screenUnlock();

    int rows = wifiNetworkCount;
    if (rows > MAX_VISIBLE_ROWS)
        rows = MAX_VISIBLE_ROWS;

    for (int i = 0; i < rows; i++)
    {
        drawNetworkRow(i, wifiNetworkNames[i], wifiNetworkRSSI[i], i == selectedWifiIndex);
    }
}

void uiWifiDrawBase()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillScreen(TFT_BLACK);
    screenUnlock();

    drawTitle();
    drawStatus();
    uiWifiRedrawList();
    drawButtons();
}

static int getTouchedRow(uint16_t x, uint16_t y)
{
    if (x < LIST_X || x >= (LIST_X + LIST_W))
        return -1;

    if (y < LIST_Y || y >= (LIST_Y + ROW_H * MAX_VISIBLE_ROWS))
        return -1;

    int row = (y - LIST_Y) / ROW_H;
    if (row < 0 || row >= wifiNetworkCount || row >= MAX_VISIBLE_ROWS)
        return -1;

    return row;
}

UiWifiAction uiWifiHandleTouch(uint16_t x, uint16_t y, int &selectedIndex)
{
    selectedIndex = -1;

    if (btnScan.contains(x, y))
    {
        screenLock();
        btnScan.drawButton(true);
        screenUnlock();

        waitTouchRelease();

        screenLock();
        btnScan.drawButton(false);
        screenUnlock();

        return UI_WIFI_ACTION_SCAN;
    }

    if (btnBack.contains(x, y))
    {
        screenLock();
        btnBack.drawButton(true);
        screenUnlock();

        waitTouchRelease();

        screenLock();
        btnBack.drawButton(false);
        screenUnlock();

        return UI_WIFI_ACTION_BACK;
    }

    int row = getTouchedRow(x, y);
    if (row >= 0)
    {
        selectedIndex = row;
        selectedWifiIndex = row;
        uiWifiRedrawList();
        waitTouchRelease();
        return UI_WIFI_ACTION_SELECT_NETWORK;
    }

    return UI_WIFI_ACTION_NONE;
}