#include "app_runtime_state.h"

volatile bool wifiReady = false;
volatile bool micEnabled = false;
volatile bool awaitingResponse = false;
volatile bool isPlayingResponse = false;
volatile bool streamFinished = true;
volatile bool micStreamActive = false;

bool networkServicesStarted = false;