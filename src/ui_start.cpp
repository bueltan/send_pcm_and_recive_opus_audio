#include "ui_start.h"

#include <TFT_eSPI.h>
#include <WiFi.h>

#include "display_hal.h"
#include "app_runtime_state.h"
#include "app_state.h"

static TFT_eSPI_Button btnMic;
static volatile UiState uiState = UI_READY;

static constexpr int MAIN_Y_OFFSET = 15;

static constexpr int SETUP_X = 188;
static constexpr int SETUP_Y = 14 + MAIN_Y_OFFSET;
static constexpr int SETUP_W = 36;
static constexpr int SETUP_H = 36;

static constexpr int MIC_BTN_CX = 120;
static constexpr int MIC_BTN_CY = 235 + MAIN_Y_OFFSET;
static constexpr int MIC_BTN_W = 140;
static constexpr int MIC_BTN_H = 78;

static const uint8_t micIcon32x32[] = {
    0x00, 0x07, 0xE0, 0x00, //         ██████
    0x00, 0x1F, 0xF8, 0x00, //       ██████████
    0x00, 0x3F, 0xFC, 0x00, //      ████████████
    0x00, 0x7F, 0xFE, 0x00, //     ██████████████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x78, 0x1E, 0x00, //     ████      ████
    0x00, 0x7F, 0xFE, 0x00, //     ██████████████
    0x00, 0x3F, 0xFC, 0x00, //      ████████████
    0x00, 0x1F, 0xF8, 0x00, //       ██████████
    0x00, 0x07, 0xE0, 0x00, //         ██████
    0x00, 0x07, 0xE0, 0x00,
    0x00, 0x07, 0xE0, 0x00,
    0x00, 0x07, 0xE0, 0x00,
    0x00, 0x07, 0xE0, 0x00,
    0x00, 0x1F, 0xF8, 0x00, //       ██████████
    0x00, 0x3F, 0xFC, 0x00, //      ████████████
    0x00, 0x07, 0xE0, 0x00, //         ██████
    0x00, 0x07, 0xE0, 0x00,
    0x00, 0x0F, 0xF0, 0x00, //        ████████
    0x00, 0x1C, 0x38, 0x00, //       ███    ███
    0x00, 0x38, 0x1C, 0x00, //      ███      ███
    0x00, 0x70, 0x0E, 0x00, //     ███        ███
    0x00, 0x60, 0x06, 0x00, //     ██          ██
    0x00, 0x7F, 0xFE, 0x00, //     ██████████████
    0x00, 0x3F, 0xFC, 0x00, //      ████████████
    0x00, 0x00, 0x00, 0x00};
    
static const uint8_t speakerIcon32x32[] = {
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x03,0x00,0x00,
    0x00,0x07,0x80,0x00,
    0x00,0x0F,0xC0,0x60,
    0x00,0x1F,0xE0,0xF0,
    0x00,0x3F,0xF0,0xF8,
    0x00,0x7F,0xF8,0x7C,
    0x00,0xFF,0xFC,0x1E,
    0x01,0xFF,0xFE,0x0E,
    0x03,0xFF,0xFF,0x06,
    0x07,0xFF,0xFF,0x06,
    0x0F,0xFF,0xFF,0x06,
    0x1F,0xFF,0xFF,0x06,
    0x1F,0xFF,0xFF,0x06,
    0x0F,0xFF,0xFF,0x06,
    0x07,0xFF,0xFF,0x06,
    0x03,0xFF,0xFF,0x06,
    0x01,0xFF,0xFE,0x0E,
    0x00,0xFF,0xFC,0x1E,
    0x00,0x7F,0xF8,0x7C,
    0x00,0x3F,0xF0,0xF8,
    0x00,0x1F,0xE0,0xF0,
    0x00,0x0F,0xC0,0x60,
    0x00,0x07,0x80,0x00,
    0x00,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00
};

static const uint8_t xIcon16x16[] = {
    0b11000000, 0b00000011,
    0b01100000, 0b00000110,
    0b00110000, 0b00001100,
    0b00011000, 0b00011000,
    0b00001100, 0b00110000,
    0b00000110, 0b01100000,
    0b00000011, 0b11000000,
    0b00000001, 0b10000000,
    0b00000001, 0b10000000,
    0b00000011, 0b11000000,
    0b00000110, 0b01100000,
    0b00001100, 0b00110000,
    0b00011000, 0b00011000,
    0b00110000, 0b00001100,
    0b01100000, 0b00000110,
    0b11000000, 0b00000011};
static const uint8_t setupIcon16x16[] = {
    0b00000111, 0b11100000, //     ███████
    0b00001111, 0b11110000, //    █████████
    0b00011000, 0b00011000, //   ██      ██
    0b00110011, 0b11001100, //  ██  ████  ██
    0b00100111, 0b11100100, //  █  ███████  █
    0b01100110, 0b01100110, // ██  ██  ██  ██
    0b01101100, 0b00110110, // ██ ██    ██ ██
    0b11111100, 0b00111111, // ███████  ███████
    0b11111100, 0b00111111, // ███████  ███████
    0b01101100, 0b00110110, // ██ ██    ██ ██
    0b01100110, 0b01100110, // ██  ██  ██  ██
    0b00100111, 0b11100100, //  █  ███████  █
    0b00110011, 0b11001100, //  ██  ████  ██
    0b00011000, 0b00011000, //   ██      ██
    0b00001111, 0b11110000, //    █████████
    0b00000111, 0b11100000  //     ███████
};
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
    tft.setCursor(18, 22 + MAIN_Y_OFFSET);
    tft.print("HAND-WALL-E");
    screenUnlock();
}

static void drawWifiStatus()
{
    TFT_eSPI &tft = display();

    uint16_t dotColor = TFT_WHITE;

    if (wifiStatus == DISCONNECTED)
    {
        dotColor = TFT_WHITE;
    }
    else if (udpStatus == UDP_CONNECTED)
    {
        dotColor = TFT_GREEN;
    }
    else
    {
        dotColor = TFT_ORANGE;
    }

    String shownSSID;

    if (wifiStatus != DISCONNECTED && WiFi.status() == WL_CONNECTED)
    {
        shownSSID = WiFi.SSID();
    }
    else if (wifiSSID.length() > 0)
    {
        shownSSID = wifiSSID;
    }
    else
    {
        shownSSID = "<none>";
    }

    if (shownSSID.length() > 10)
    {
        shownSSID = shownSSID.substring(0, 10) + "...";
    }

    String label = "WiFi: " + shownSSID;

    static constexpr int WIFI_BOX_X = 6;
    static constexpr int WIFI_BOX_Y = 6;
    static constexpr int WIFI_BOX_W = 130;
    static constexpr int WIFI_BOX_H = 16;

    static constexpr int WIFI_TEXT_X = 8;
    static constexpr int WIFI_TEXT_Y = 10;
    static constexpr int WIFI_DOT_R = 4;
    static constexpr int WIFI_DOT_GAP = 8;

    screenLock();

    tft.fillRect(WIFI_BOX_X, WIFI_BOX_Y, WIFI_BOX_W, WIFI_BOX_H, TFT_BLACK);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(WIFI_TEXT_X, WIFI_TEXT_Y);
    tft.print(label);

    int textWidth = tft.textWidth(label);
    int dotX = WIFI_TEXT_X + textWidth + WIFI_DOT_GAP;
    int dotY = WIFI_BOX_Y + WIFI_BOX_H / 2;

    int maxDotX = WIFI_BOX_X + WIFI_BOX_W - WIFI_DOT_R - 1;
    if (dotX > maxDotX)
    {
        dotX = maxDotX;
    }

    tft.fillCircle(dotX, dotY, WIFI_DOT_R, dotColor);
    tft.drawCircle(dotX, dotY, WIFI_DOT_R, TFT_GREEN);

    screenUnlock();
}

static void drawStatusText(const char *line1, const char *line2 = nullptr)
{
    TFT_eSPI &tft = display();

    screenLock();
    tft.fillRect(10, 70 + MAIN_Y_OFFSET, 220, 90, TFT_BLACK);
    tft.drawRect(10, 70 + MAIN_Y_OFFSET, 220, 90, TFT_GREEN);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(18, 88 + MAIN_Y_OFFSET);
    tft.print(line1 ? line1 : "");

    if (line2)
    {
        tft.setTextSize(1);
        tft.setCursor(18, 126 + MAIN_Y_OFFSET);
        tft.print(line2);
    }

    screenUnlock();
}

static void drawMicButton(bool inverted = false)
{
    TFT_eSPI &tft = display();

    const uint16_t bg = inverted ? TFT_GREEN : TFT_BLACK;
    const uint16_t fg = inverted ? TFT_BLACK : TFT_GREEN;

    const int x = MIC_BTN_CX - MIC_BTN_W / 2;
    const int y = MIC_BTN_CY - MIC_BTN_H / 2;

    const uint8_t *mainIcon = micIcon32x32;
    int mainIconW = 32;
    int mainIconH = 32;

    if (uiState == UI_SPEAKING)
    {
        mainIcon = speakerIcon32x32;
    }

    screenLock();

    btnMic.initButton(
        &tft,
        MIC_BTN_CX, MIC_BTN_CY,
        MIC_BTN_W, MIC_BTN_H,
        TFT_GREEN, TFT_BLACK, TFT_GREEN,
        (char *)"",
        2);

    tft.fillRoundRect(x, y, MIC_BTN_W, MIC_BTN_H, 10, bg);
    tft.drawRoundRect(x, y, MIC_BTN_W, MIC_BTN_H, 10, TFT_GREEN);

    const int iconX = MIC_BTN_CX - mainIconW / 2;
    const int iconY = MIC_BTN_CY - mainIconH / 2;
    tft.drawBitmap(iconX, iconY, mainIcon, mainIconW, mainIconH, fg);

    if (uiState == UI_ERROR)
    {
        const int xIconX = MIC_BTN_CX + 10;
        const int xIconY = MIC_BTN_CY - 20;
        tft.drawBitmap(xIconX, xIconY, xIcon16x16, 16, 16, fg);
    }

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
    drawWifiStatus();
    drawStatusText("Ready", "Tap MIC to talk");
    drawMicButton(false);
}

void uiStartApplyState(UiState state)
{
    if (state == uiState)
    {
        drawWifiStatus();

        if (state == UI_ERROR)
        {
            drawStatusText(lastErrorLine1.c_str(), lastErrorLine2.c_str());
        }

        drawMicButton(state == UI_LISTENING);
        return;
    }

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
        drawStatusText(lastErrorLine1.c_str(), lastErrorLine2.c_str());
        break;
    }

    drawMicButton(state == UI_LISTENING);
    drawSetupButton(false);
    drawWifiStatus();
}

void uiStartDrawWifiStatus()
{
    drawWifiStatus();
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