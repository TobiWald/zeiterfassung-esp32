#pragma once
#include <Arduino.h>
#include <time.h>

// Systemzeit = UTC-Epoche, Darstellung über TZ (Europe/Berlin).
namespace timeutil {
void begin();               // TZ setzen, Uhrzeit aus RTC übernehmen
bool valid();               // ist die Uhrzeit plausibel gesetzt?
time_t now();
void set(time_t utc);       // System + RTC setzen
void saveToRtc();           // aktuelle Systemzeit in die RTC schreiben

// Lokale Kalender-Hilfen
time_t dayStart(time_t t);
time_t addDays(time_t dayStartT, int days);
time_t weekStart(time_t t);  // Montag 00:00
time_t monthStart(time_t t);
time_t monthStart(int year, int month);  // month 1..12
time_t addMonths(time_t monthStartT, int months);
struct tm local(time_t t);
int isoWeek(time_t t, int *isoYear = nullptr);

String fmtDuration(uint32_t sec);     // "7:05"
String fmtClock(time_t t);            // "08:15"
String fmtDate(time_t t);             // "01.10.2026"
const char *weekdayShort(int wday);   // 0=So
const char *monthName(int month);     // 1..12
}
