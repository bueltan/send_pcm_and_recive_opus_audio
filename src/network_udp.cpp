#include "network_udp.h"

#include <Arduino.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

#include "app_state.h"

static WiFiUDP udp;

static bool isExactControlMessage(const uint8_t *data, int len, const char *text)
{
    if (data == nullptr || text == nullptr || len <= 0)
        return false;

    const size_t expectedLen = strlen(text);
    if (len != (int)expectedLen)
        return false;

    return memcmp(data, text, expectedLen) == 0;
}

bool networkUdpInit(uint16_t localPort)
{
    return udp.begin(localPort) == 1;
}

bool networkUdpSendRaw(const char *serverIp, uint16_t serverPort, const uint8_t *data, size_t len)
{
    if (udpMutex == nullptr)
        return false;

    if (xSemaphoreTake(udpMutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return false;

    udp.beginPacket(serverIp, serverPort);
    udp.write(data, len);
    const int ok = udp.endPacket();

    xSemaphoreGive(udpMutex);
    return ok == 1;
}

int networkUdpParsePacket()
{
    if (udpMutex == nullptr)
        return 0;

    if (xSemaphoreTake(udpMutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return 0;

    const int packetSize = udp.parsePacket();

    xSemaphoreGive(udpMutex);
    return packetSize;
}

int networkUdpRead(uint8_t *buffer, size_t bufferSize)
{
    if (udpMutex == nullptr || buffer == nullptr || bufferSize == 0)
        return 0;

    if (xSemaphoreTake(udpMutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return 0;

    const int n = udp.read(buffer, bufferSize);

    xSemaphoreGive(udpMutex);

    if (n <= 0)
        return 0;

    // Heartbeat response from backend
    if (isExactControlMessage(buffer, n, "PONG"))
    {
        lastPongTime = millis();
        udpStatus = UDP_CONNECTED;
        return 0; // swallow control packet
    }

    return n;
}