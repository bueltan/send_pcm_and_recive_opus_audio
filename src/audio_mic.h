#pragma once

#include <Arduino.h>

typedef bool (*AudioMicRawUdpSender)(const uint8_t *data, size_t len);

void audioMicSetRawUdpSender(AudioMicRawUdpSender sender);

bool audioMicInit();
void audioMicStartTask();
void audioMicResetUplinkState();
void audioMicHandleLoop();