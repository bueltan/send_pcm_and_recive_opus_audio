#pragma once

void startTouchTask();

bool takeToggleMicRequest();
bool takeOpenSetupRequest();

bool takeSetupWifiRequest();
bool takeSetupServerRequest();
bool takeSetupTouchRequest();
bool takeSetupBackRequest();

bool takeWifiScanRequest();
bool takeWifiBackRequest();
bool takeWifiSelectRequest(int &index);