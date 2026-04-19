#pragma once

#include <Arduino.h>

bool networkUdpInit(uint16_t localPort);
bool networkUdpSendRaw(const char *serverIp, uint16_t serverPort, const uint8_t *data, size_t len);

int networkUdpParsePacket();
int networkUdpRead(uint8_t *buffer, size_t bufferSize);