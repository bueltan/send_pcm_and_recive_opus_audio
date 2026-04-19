#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

extern volatile bool commitRequested;
extern volatile bool cancelRequested;

extern SemaphoreHandle_t screenMutex;
extern SemaphoreHandle_t udpMutex;

enum AppScreen
{
    SCREEN_START,
    SCREEN_SETUP,
    SCREEN_DEV
};

enum Status
{
    DISCONNECTED,
    WIFI_CONNECTED,
    UDP_CONNECTED
};

struct UdpPacket_t
{
    uint8_t *data;
    size_t length;
    bool isAudio;
};

struct AudioHeader
{
    uint32_t sequence;
} __attribute__((packed));