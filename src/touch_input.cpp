#include "touch_input.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_state.h"
#include "display_hal.h"
#include "ui_setup.h"
#include "ui_start.h"
#include "ui_wifi.h"

static volatile bool toggleMicRequested = false;
static volatile bool openSetupRequested = false;

static volatile bool setupWifiRequested = false;
static volatile bool setupServerRequested = false;
static volatile bool setupTouchRequested = false;
static volatile bool setupBackRequested = false;

static volatile bool wifiScanRequested = false;
static volatile bool wifiBackRequested = false;
static volatile bool wifiSelectRequested = false;
static volatile int wifiSelectedIndexRequested = -1;

static void handleStartScreenTouch(uint16_t x, uint16_t y)
{
    UiStartAction action = uiStartHandleTouch(x, y);

    if (action == UI_ACTION_MIC)
    {
        toggleMicRequested = true;
    }
    else if (action == UI_ACTION_SETUP)
    {
        openSetupRequested = true;
    }
    else
    {
        waitTouchRelease();
    }
}

static void handleSetupScreenTouch(uint16_t x, uint16_t y)
{
    UiSetupAction action = uiSetupHandleTouch(x, y);

    if (action == UI_SETUP_ACTION_WIFI)
    {
        setupWifiRequested = true;
    }
    else if (action == UI_SETUP_ACTION_SERVER)
    {
        setupServerRequested = true;
    }
    else if (action == UI_SETUP_ACTION_TOUCH)
    {
        setupTouchRequested = true;
    }
    else if (action == UI_SETUP_ACTION_BACK)
    {
        setupBackRequested = true;
    }
    else
    {
        waitTouchRelease();
    }
}

static void handleWifiScreenTouch(uint16_t x, uint16_t y)
{
    int selectedIndex = -1;
    UiWifiAction action = uiWifiHandleTouch(x, y, selectedIndex);

    if (action == UI_WIFI_ACTION_SCAN)
    {
        wifiScanRequested = true;
    }
    else if (action == UI_WIFI_ACTION_BACK)
    {
        wifiBackRequested = true;
    }
    else if (action == UI_WIFI_ACTION_SELECT_NETWORK)
    {
        wifiSelectedIndexRequested = selectedIndex;
        wifiSelectRequested = true;
    }
    else
    {
        waitTouchRelease();
    }
}

static void taskTouch(void *parameter)
{
    (void)parameter;

    while (true)
    {
        uint16_t x, y;

        if (getTouchPoint(x, y))
        {
            if (currentScreen == SCREEN_START)
            {
                handleStartScreenTouch(x, y);
            }
            else if (currentScreen == SCREEN_SETUP)
            {
                handleSetupScreenTouch(x, y);
            }
            else if (currentScreen == SCREEN_WIFI)
            {
                handleWifiScreenTouch(x, y);
            }
            else
            {
                waitTouchRelease();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void startTouchTask()
{
    xTaskCreatePinnedToCore(taskTouch, "taskTouch", 3072, nullptr, 1, nullptr, 1);
}

bool takeToggleMicRequest()
{
    if (!toggleMicRequested)
        return false;

    toggleMicRequested = false;
    return true;
}

bool takeOpenSetupRequest()
{
    if (!openSetupRequested)
        return false;

    openSetupRequested = false;
    return true;
}

bool takeSetupWifiRequest()
{
    if (!setupWifiRequested)
        return false;

    setupWifiRequested = false;
    return true;
}

bool takeSetupServerRequest()
{
    if (!setupServerRequested)
        return false;

    setupServerRequested = false;
    return true;
}

bool takeSetupTouchRequest()
{
    if (!setupTouchRequested)
        return false;

    setupTouchRequested = false;
    return true;
}

bool takeSetupBackRequest()
{
    if (!setupBackRequested)
        return false;

    setupBackRequested = false;
    return true;
}

bool takeWifiScanRequest()
{
    if (!wifiScanRequested)
        return false;

    wifiScanRequested = false;
    return true;
}

bool takeWifiBackRequest()
{
    if (!wifiBackRequested)
        return false;

    wifiBackRequested = false;
    return true;
}

bool takeWifiSelectRequest(int &index)
{
    if (!wifiSelectRequested)
        return false;

    index = wifiSelectedIndexRequested;
    wifiSelectRequested = false;
    wifiSelectedIndexRequested = -1;
    return true;
}