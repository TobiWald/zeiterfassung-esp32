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

bool addSession(uint32_t start, uint32_t end);
bool deleteSession(uint32_t start);

// Gearbeitete Sekunden im Intervall [from, to), inkl. laufender Sitzung
uint32_t workedBetween(uint32_t from, uint32_t to, uint32_t now);
}
