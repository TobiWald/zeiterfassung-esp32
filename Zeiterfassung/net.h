#pragma once
#include <Arduino.h>

namespace net {
void begin();
void loop();

// Heim-WLAN (Uhrzeit per NTP, Webseite auch im Heimnetz erreichbar)
void setSta(bool on);
bool staWanted();       // vom Benutzer eingeschaltet
bool staActive();       // Funk aktiv (auch temporär für Zeitsync)
bool staConnected();
String staIp();

// Eigener Access Point mit Captive Portal
void setAp(bool on);
bool apOn();
String apIp();

// Temporär mit Heim-WLAN verbinden, Uhrzeit holen, wieder trennen
void requestTimeSync();
bool timeSyncPending();
bool syncTimeBlocking(uint32_t timeoutMs);
uint32_t lastSyncEpoch();

// Kurze Meldung für das Display (leer = keine)
String takeNotice();
}
