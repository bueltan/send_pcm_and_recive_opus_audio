#pragma once

extern volatile bool wifiReady;
extern volatile bool micEnabled;
extern volatile bool awaitingResponse;
extern volatile bool isPlayingResponse;
extern volatile bool streamFinished;
extern volatile bool micStreamActive;

// Network/audio service lifecycle
extern bool networkServicesStarted;