#pragma once

#include <Arduino.h>

extern volatile bool wifiReady;

extern volatile bool micEnabled;
extern volatile bool micStreamActive;
extern volatile bool awaitingResponse;
extern volatile bool isPlayingResponse;
extern volatile bool streamFinished;