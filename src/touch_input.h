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

bool takeWifiPasswordBackRequest();
bool takeWifiPasswordOkRequest();

bool takeServerAddRequest();
bool takeServerBackRequest();
bool takeServerOkRequest();
bool takeServerSelectRequest(int &index);

bool takeServerEditRequest();

bool takeServerEditBackRequest();
bool takeServerEditOkRequest();
bool takeServerDeleteRequest();