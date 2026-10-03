#pragma once
#include <Arduino.h>
#include <vector>

namespace net {
struct Network {
  String ssid;
  String pass;
};
struct ScanResult {
  String ssid;
  int rssi;
  bool secure;
};

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

// WLAN-Verwaltung (Webseite). Das fest eingebaute Netz aus secrets.h
// bleibt immer als Rückfall erhalten.
std::vector<Network> savedNetworks();
String currentSsid();
bool addNetwork(const String &ssid, const String &pass);  // verbindet ggf. sofort
bool removeNetwork(const String &ssid);
int scanStart();                       // <0 = läuft noch, >=0 = fertig
std::vector<ScanResult> scanResults(); // holt und verwirft das Ergebnis

// Kurze Meldung für das Display (leer = keine)
String takeNotice();
}
