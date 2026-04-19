#include "network_udp.h"

#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "app_state.h"

static WiFiUDP udp;

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
    int ok = udp.endPacket();

    xSemaphoreGive(udpMutex);
    return ok == 1;
}

int networkUdpParsePacket()
{
    if (udpMutex == nullptr)
        return 0;

    if (xSemaphoreTake(udpMutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return 0;

    int packetSize = udp.parsePacket();

    xSemaphoreGive(udpMutex);
    return packetSize;
}

int networkUdpRead(uint8_t *buffer, size_t bufferSize)
{
    if (udpMutex == nullptr)
        return 0;

    if (xSemaphoreTake(udpMutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return 0;

    int n = udp.read(buffer, bufferSize);

    xSemaphoreGive(udpMutex);
    return n;
}