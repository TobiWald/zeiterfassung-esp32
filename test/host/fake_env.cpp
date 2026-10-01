// Ersatz für Hardware/Netz/Speicher in Host-Tests
#include <Arduino.h>
#include <vector>
#include <algorithm>
#include "../../Zeiterfassung/storage.h"
#include "../../Zeiterfassung/net.h"
#include "../../Zeiterfassung/app.h"
#include "../../Zeiterfassung/timeutil.h"

uint32_t g_millis = 0;
uint32_t millis() { return g_millis; }
time_t g_now = 0;
bool g_ap = false, g_sta = false, g_staConn = false;
int g_bat = 76;

namespace { std::vector<Session> L; uint32_t run = 0; }
namespace storage {
void begin() {}
const std::vector<Session> &sessions() { return L; }
bool running() { return run; }
uint32_t runningSince() { return run; }
void start(uint32_t n) { run = n; }
void stop(uint32_t n) { L.push_back({run, n}); run = 0; }
bool addSession(uint32_t s, uint32_t e) { L.push_back({s, e}); std::sort(L.begin(), L.end(), [](auto&a, auto&b){return a.start<b.start;}); return true; }
bool deleteSession(uint32_t) { return false; }
uint32_t workedBetween(uint32_t from, uint32_t to, uint32_t now) {
  auto ov = [](uint32_t s, uint32_t e, uint32_t f, uint32_t t) { uint32_t a = std::max(s, f), b = std::min(e, t); return b > a ? b - a : 0u; };
  uint32_t sum = 0; for (auto &s : L) sum += ov(s.start, s.end, from, to);
  if (run && now > run) sum += ov(run, now, from, to);
  return sum;
}
}
namespace net {
bool apOn() { return g_ap; } bool staActive() { return g_sta; } bool staConnected() { return g_staConn; }
}
namespace app { float batteryVoltage() { return 3.9f; } int batteryPercent() { return g_bat; } void requestRedraw() {} void toggleTracking() {} }
namespace epd {
uint8_t shown[5000];
void begin() {}
void fullRefresh(const uint8_t *b) { memcpy(shown, b, 5000); }
void partialRefresh(const uint8_t *b) { memcpy(shown, b, 5000); }
}
// timeutil ohne RTC
namespace timeutil { void begin(); }
