#pragma once
#include <Arduino.h>
#include <WebServer.h>

namespace exporter {
// Zeitraum [from, to) in UTC-Epoche. monthMode: alle Kalendertage auflisten.
struct Range {
  uint32_t from;
  uint32_t to;
  bool monthMode;
  String fileLabel;  // z.B. "2026-10" oder "gesamt"
};

Range rangeForMonth(int year, int month);
Range rangeAll(uint32_t now);

void sendXlsx(WebServer &srv, const Range &r, uint32_t now);
void sendCsv(WebServer &srv, const Range &r, uint32_t now);
}
