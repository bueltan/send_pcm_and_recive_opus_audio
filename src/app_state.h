#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

extern volatile bool commitRequested;
extern volatile bool cancelRequested;

extern QueueHandle_t udpAudioQueue;
extern QueueHandle_t udpControlQueue;

// ====================== APP SCREEN ======================
enum AppScreen
{
    SCREEN_START,
    SCREEN_SETUP,
    SCREEN_WIFI,
    SCREEN_WIFI_PASSWORD,
};

extern volatile AppScreen currentScreen;

// ====================== NETWORK STATUS ======================
enum Status
{
    DISCONNECTED,
    WIFI_CONNECTED,
    UDP_CONNECTED
};

extern volatile Status wifiStatus;
extern volatile Status udpStatus;
extern volatile bool micStreaming;

// ====================== CONFIG VALUES ======================
extern String wifiSSID;
extern String wifiPASS;
extern String serverIP;

// ====================== NETWORK STATE ======================
extern unsigned long lastPingTime;
extern unsigned long lastPongTime;
extern bool udpSocketStarted;

// ====================== SHARED RTOS OBJECTS ======================
extern SemaphoreHandle_t screenMutex;
extern SemaphoreHandle_t udpMutex;

// ====================== CONFIG SCREEN STATE ======================
extern String inputText;
extern bool capsLock;
extern bool symbolMode;
extern int CURRENT_STAGE;
extern String currentTitle;

// ====================== TOUCH ======================
extern uint16_t t_x;
extern uint16_t t_y;

// ====================== AUDIO ======================
extern uint32_t sequence;

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

// ====================== WIFI SETUP ======================

constexpr int MAX_WIFI_NETWORKS = 10;

extern int wifiNetworkCount;
extern String wifiNetworkNames[MAX_WIFI_NETWORKS];
extern int wifiNetworkRSSI[MAX_WIFI_NETWORKS];
extern bool wifiScanInProgress;
extern int selectedWifiIndex;


extern String lastErrorLine1;
extern String lastErrorLine2;