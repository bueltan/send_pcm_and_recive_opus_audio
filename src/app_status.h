#pragma once

#include <Arduino.h>

void appStatusInit();

void appStatusSetUiError(const char *line1, const char *line2);

void appStatusResetServerHeartbeat();
void appStatusUpdateServerHeartbeat();

bool appStatusStartNetworkServicesIfNeeded();
void appStatusRefreshConnectivity();
void appStatusRefreshStartScreen();