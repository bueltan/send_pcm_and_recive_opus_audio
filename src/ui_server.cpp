#include "ui_server.h"

#include <TFT_eSPI.h>

#include "app_state.h"
#include "display_hal.h"

static TFT_eSPI_Button btnAdd;
static TFT_eSPI_Button btnEdit;
static TFT_eSPI_Button btnOk;
static TFT_eSPI_Button btnBack;

static constexpr int LIST_X = 10;
static constexpr int LIST_Y = 52;
static constexpr int LIST_W = 220;
static constexpr int ROW_H = 24;
static constexpr int MAX_VISIBLE_ROWS = 7;

static TFT_eSPI_Button btnDelete;

static void drawTitle()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(74, 16);
    tft.print("SERVER");
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
    tft.print("Saved: ");
    tft.print(serverListCount);
    screenUnlock();
}

static void drawButtons()
{
    TFT_eSPI &tft = display();

    screenLock();

    btnAdd.initButton(&tft, 24, 295, 40, 34, TFT_GREEN, TFT_BLACK, TFT_GREEN, (char *)"ADD", 2);
    btnEdit.initButton(&tft, 72, 295, 48, 34, TFT_GREEN, TFT_BLACK, TFT_GREEN, (char *)"EDIT", 2);
    btnDelete.initButton(&tft, 126, 295, 54, 34, TFT_GREEN, TFT_BLACK, TFT_GREEN, (char *)"DEL", 2);
    btnOk.initButton(&tft, 178, 295, 40, 34, TFT_GREEN, TFT_BLACK, TFT_GREEN, (char *)"OK", 2);
    btnBack.initButton(&tft, 218, 295, 44, 34, TFT_GREEN, TFT_BLACK, TFT_GREEN, (char *)"BACK", 2);

    btnAdd.drawButton(false);
    btnEdit.drawButton(false);
    btnDelete.drawButton(false);
    btnOk.drawButton(false);
    btnBack.drawButton(false);

    screenUnlock();
}
static void drawServerRow(int row, const String &value, bool selected)
{
    TFT_eSPI &tft = display();

    const int y = LIST_Y + row * ROW_H;
    const uint16_t bg = selected ? TFT_GREEN : TFT_BLACK;
    const uint16_t fg = selected ? TFT_BLACK : TFT_GREEN;

    String shown = value;
    if (shown.length() > 20)
    {
        shown = shown.substring(0, 20);
    }

    screenLock();

    tft.fillRect(LIST_X, y, LIST_W, ROW_H - 2, bg);
    tft.drawRect(LIST_X, y, LIST_W, ROW_H - 2, TFT_GREEN);

    tft.setTextColor(fg, bg);
    tft.setTextSize(1);
    tft.setCursor(LIST_X + 6, y + 6);
    tft.print(shown);

    screenUnlock();
}

void uiServerInit()
{
}

void uiServerRedrawList()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillRect(LIST_X, LIST_Y, LIST_W, ROW_H * MAX_VISIBLE_ROWS, TFT_BLACK);
    screenUnlock();

    int rows = serverListCount;
    if (rows > MAX_VISIBLE_ROWS)
        rows = MAX_VISIBLE_ROWS;

    for (int i = 0; i < rows; i++)
    {
        drawServerRow(i, serverList[i], i == selectedServerIndex);
    }
}

void uiServerDrawBase()
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillScreen(TFT_BLACK);
    screenUnlock();

    drawTitle();
    drawStatus();
    uiServerRedrawList();
    drawButtons();
}

static int getTouchedRow(uint16_t x, uint16_t y)
{
    if (x < LIST_X || x >= (LIST_X + LIST_W))
        return -1;

    if (y < LIST_Y || y >= (LIST_Y + ROW_H * MAX_VISIBLE_ROWS))
        return -1;

    int row = (y - LIST_Y) / ROW_H;
    if (row < 0 || row >= serverListCount || row >= MAX_VISIBLE_ROWS)
        return -1;

    return row;
}

UiServerAction uiServerHandleTouch(uint16_t x, uint16_t y, int &selectedIndex)
{
    selectedIndex = -1;

    if (btnAdd.contains(x, y))
    {
        btnAdd.drawButton(true);
        waitTouchRelease();
        btnAdd.drawButton(false);
        return UI_SERVER_ACTION_ADD;
    }

    if (btnEdit.contains(x, y))
    {
        btnEdit.drawButton(true);
        waitTouchRelease();
        btnEdit.drawButton(false);
        return UI_SERVER_ACTION_EDIT;
    }

    if (btnOk.contains(x, y))
    {
        btnOk.drawButton(true);
        waitTouchRelease();
        btnOk.drawButton(false);
        return UI_SERVER_ACTION_OK;
    }

    if (btnBack.contains(x, y))
    {
        btnBack.drawButton(true);
        waitTouchRelease();
        btnBack.drawButton(false);
        return UI_SERVER_ACTION_BACK;
    }

    int row = getTouchedRow(x, y);
    if (row >= 0)
    {
        selectedIndex = row;
        selectedServerIndex = row;
        uiServerRedrawList();
        waitTouchRelease();
        return UI_SERVER_ACTION_SELECT;
    }
    if (btnDelete.contains(x, y))
    {
        btnDelete.drawButton(true);
        waitTouchRelease();
        btnDelete.drawButton(false);
        return UI_SERVER_ACTION_DELETE;
    }

    return UI_SERVER_ACTION_NONE;
}