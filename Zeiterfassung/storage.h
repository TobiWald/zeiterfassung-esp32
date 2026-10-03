#pragma once
#include <Arduino.h>
#include <vector>

struct Session {
  uint32_t start;  // UTC-Epoche
  uint32_t end;
};

namespace storage {
void begin();
const std::vector<Session> &sessions();  // sortiert nach Start

bool running();
uint32_t runningSince();
void start(uint32_t now);
void stop(uint32_t now);  // beendet laufende Sitzung und speichert sie
void setRunningSince(uint32_t start);  // Startzeit der laufenden Sitzung ändern
uint32_t lastEnd();                    // Ende des letzten gespeicherten Eintrags (0 = keiner)

bool addSession(uint32_t start, uint32_t end);
bool deleteSession(uint32_t start);
bool clearAll();  // alle Einträge löschen, laufende Erfassung beenden

// Gearbeitete Sekunden im Intervall [from, to), inkl. laufender Sitzung
uint32_t workedBetween(uint32_t from, uint32_t to, uint32_t now);
}
