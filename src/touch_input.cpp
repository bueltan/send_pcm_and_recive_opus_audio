#include "touch_input.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display_hal.h"
#include "ui_start.h"

static volatile bool toggleMicRequested = false;

static void taskTouch(void *parameter)
{
    (void)parameter;

    while (true)
    {
        uint16_t x, y;

        if (getTouchPoint(x, y))
        {
            if (uiStartHandleTouch(x, y))
            {
                toggleMicRequested = true;
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