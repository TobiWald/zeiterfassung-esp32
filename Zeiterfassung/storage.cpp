#include "storage.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <algorithm>

namespace {

const char *kFile = "/sessions.bin";
const char *kTmp = "/sessions.tmp";

std::vector<Session> list;
uint32_t runStart = 0;
Preferences prefs;

void sortList() {
  std::sort(list.begin(), list.end(), [](const Session &a, const Session &b) { return a.start < b.start; });
}

bool save() {
  File f = LittleFS.open(kTmp, "w");
  if (!f) return false;
  size_t n = list.size() * sizeof(Session);
  bool ok = n == 0 || f.write((const uint8_t *)list.data(), n) == n;
  f.close();
  if (!ok) return false;
  LittleFS.remove(kFile);
  return LittleFS.rename(kTmp, kFile);
}

uint32_t overlap(uint32_t s, uint32_t e, uint32_t from, uint32_t to) {
  uint32_t a = s > from ? s : from;
  uint32_t b = e < to ? e : to;
  return b > a ? b - a : 0;
}

}  // namespace

namespace storage {

void begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS konnte nicht eingebunden werden");
  }
  list.clear();
  const char *path = LittleFS.exists(kFile) ? kFile : (LittleFS.exists(kTmp) ? kTmp : nullptr);
  if (path) {
    File f = LittleFS.open(path, "r");
    Session s;
    while (f && f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
      if (s.end > s.start) list.push_back(s);
    }
    f.close();
  }
  sortList();
  prefs.begin("zeit", false);
  runStart = prefs.getUInt("run", 0);
}

const std::vector<Session> &sessions() { return list; }

bool running() { return runStart != 0; }
uint32_t runningSince() { return runStart; }

void start(uint32_t now) {
  if (runStart) return;
  runStart = now;
  prefs.putUInt("run", runStart);
}

void stop(uint32_t now) {
  if (!runStart) return;
  if (now > runStart) {
    list.push_back({runStart, now});
    sortList();
    save();
  }
  runStart = 0;
  prefs.putUInt("run", 0);
}

void setRunningSince(uint32_t start) {
  if (!runStart || !start) return;
  runStart = start;
  prefs.putUInt("run", runStart);
}

uint32_t lastEnd() {
  uint32_t e = 0;
  for (const auto &s : list) e = s.end > e ? s.end : e;
  return e;
}

bool addSession(uint32_t start, uint32_t end) {
  if (end <= start) return false;
  list.push_back({start, end});
  sortList();
  return save();
}

bool deleteSession(uint32_t start) {
  auto it = std::find_if(list.begin(), list.end(), [&](const Session &s) { return s.start == start; });
  if (it == list.end()) return false;
  list.erase(it);
  return save();
}

bool clearAll() {
  list.clear();
  runStart = 0;
  prefs.putUInt("run", 0);
  return save();
}

uint32_t workedBetween(uint32_t from, uint32_t to, uint32_t now) {
  uint32_t sum = 0;
  for (const auto &s : list) {
    if (s.start >= to) break;
    sum += overlap(s.start, s.end, from, to);
  }
  if (runStart && now > runStart) sum += overlap(runStart, now, from, to);
  return sum;
}

}  // namespace storage
