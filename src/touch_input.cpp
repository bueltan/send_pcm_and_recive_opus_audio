#include "touch_input.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_state.h"
#include "display_hal.h"
#include "ui_setup.h"
#include "ui_start.h"
#include "ui_wifi.h"
#include "ui_wifi_password.h"
#include "ui_server.h"
#include "ui_server_edit.h"

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

static volatile bool wifiPasswordBackRequested = false;
static volatile bool wifiPasswordOkRequested = false;

// SERVER screen requests
static volatile bool serverAddRequested = false;
static volatile bool serverEditRequested = false;
static volatile bool serverBackRequested = false;
static volatile bool serverOkRequested = false;
static volatile bool serverSelectRequested = false;
static volatile int serverSelectedIndexRequested = -1;

// SERVER EDIT screen requests
static volatile bool serverEditBackRequested = false;
static volatile bool serverEditOkRequested = false;
static volatile bool serverDeleteRequested = false;

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

static void handleWifiPasswordScreenTouch(uint16_t x, uint16_t y)
{
    UiWifiPasswordAction action = uiWifiPasswordHandleTouch(x, y);

    if (action == UI_WIFI_PASSWORD_ACTION_BACK)
    {
        wifiPasswordBackRequested = true;
    }
    else if (action == UI_WIFI_PASSWORD_ACTION_OK)
    {
        wifiPasswordOkRequested = true;
    }
}

static void handleServerScreenTouch(uint16_t x, uint16_t y)
{
    int selectedIndex = -1;
    UiServerAction action = uiServerHandleTouch(x, y, selectedIndex);

    if (action == UI_SERVER_ACTION_ADD)
    {
        serverAddRequested = true;
    }
    else if (action == UI_SERVER_ACTION_EDIT)
    {
        serverEditRequested = true;
    }
    else if (action == UI_SERVER_ACTION_BACK)
    {
        serverBackRequested = true;
    }
    else if (action == UI_SERVER_ACTION_OK)
    {
        serverOkRequested = true;
    }
    else if (action == UI_SERVER_ACTION_SELECT)
    {
        serverSelectedIndexRequested = selectedIndex;
        serverSelectRequested = true;
    }
    else if (action == UI_SERVER_ACTION_DELETE)
    {
        serverDeleteRequested = true;
    }
    else
    {
        waitTouchRelease();
    }
}

static void handleServerEditScreenTouch(uint16_t x, uint16_t y)
{
    UiServerEditAction action = uiServerEditHandleTouch(x, y);

    if (action == UI_SERVER_EDIT_ACTION_BACK)
    {
        serverEditBackRequested = true;
    }
    else if (action == UI_SERVER_EDIT_ACTION_OK)
    {
        serverEditOkRequested = true;
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
            else if (currentScreen == SCREEN_WIFI_PASSWORD)
            {
                handleWifiPasswordScreenTouch(x, y);
            }
            else if (currentScreen == SCREEN_SERVER)
            {
                handleServerScreenTouch(x, y);
            }
            else if (currentScreen == SCREEN_SERVER_EDIT)
            {
                handleServerEditScreenTouch(x, y);
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

bool takeWifiPasswordBackRequest()
{
    if (!wifiPasswordBackRequested)
        return false;

    wifiPasswordBackRequested = false;
    return true;
}

bool takeWifiPasswordOkRequest()
{
    if (!wifiPasswordOkRequested)
        return false;

    wifiPasswordOkRequested = false;
    return true;
}

bool takeServerAddRequest()
{
    if (!serverAddRequested)
        return false;

    serverAddRequested = false;
    return true;
}

bool takeServerEditRequest()
{
    if (!serverEditRequested)
        return false;

    serverEditRequested = false;
    return true;
}

bool takeServerBackRequest()
{
    if (!serverBackRequested)
        return false;

    serverBackRequested = false;
    return true;
}

bool takeServerOkRequest()
{
    if (!serverOkRequested)
        return false;

    serverOkRequested = false;
    return true;
}

bool takeServerSelectRequest(int &index)
{
    if (!serverSelectRequested)
        return false;

    index = serverSelectedIndexRequested;
    serverSelectRequested = false;
    serverSelectedIndexRequested = -1;
    return true;
}

bool takeServerEditBackRequest()
{
    if (!serverEditBackRequested)
        return false;

    serverEditBackRequested = false;
    return true;
}

bool takeServerEditOkRequest()
{
    if (!serverEditOkRequested)
        return false;

    serverEditOkRequested = false;
    return true;
}

bool takeServerDeleteRequest()
{
    if (!serverDeleteRequested)
        return false;

    serverDeleteRequested = false;
    return true;
}