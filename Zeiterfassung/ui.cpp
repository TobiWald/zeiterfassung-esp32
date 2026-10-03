#include "ui.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <vector>
#include "app.h"
#include "config.h"
#include "epd.h"
#include "net.h"
#include "qrcodegen.h"
#include "storage.h"
#include "timeutil.h"

namespace {

const uint16_t BLACK = 0, WHITE = 1;
const GFXfont *F9 = &FreeSansBold9pt7b;
const GFXfont *F12 = &FreeSansBold12pt7b;
const GFXfont *F18 = &FreeSansBold18pt7b;
const GFXfont *F24 = &FreeSansBold24pt7b;

GFXcanvas1 canvas(EPD_W, EPD_H);
uint8_t lastFrame[EPD_BUF_SIZE];
bool haveLastFrame = false;
int partialCount = 0;

View curView = V_MAIN;
uint32_t viewSince = 0;
String msgText;
uint32_t msgUntil = 0;
bool msgActive = false;
bool msgSticky = false;
bool redrawReq = true;
uint32_t lastRender = 0;

enum Align { LEFT, CENTER, RIGHT };

// ---------------------------------------------- Text mit Umlauten (UTF-8)
// Zerlegt UTF-8 in darstellbare ASCII-Zeichen; Umlaute werden als
// Grundbuchstabe + zwei Punkte gezeichnet.
struct GlyphItem {
  char c;
  bool dots;
};

std::vector<GlyphItem> decode(const char *s) {
  std::vector<GlyphItem> out;
  for (const uint8_t *p = (const uint8_t *)s; *p; p++) {
    if (*p == 0xC3 && p[1]) {
      p++;
      switch (*p) {
        case 0xA4: out.push_back({'a', true}); break;
        case 0xB6: out.push_back({'o', true}); break;
        case 0xBC: out.push_back({'u', true}); break;
        case 0x84: out.push_back({'A', true}); break;
        case 0x96: out.push_back({'O', true}); break;
        case 0x9C: out.push_back({'U', true}); break;
        case 0x9F: out.push_back({'s', false}); out.push_back({'s', false}); break;
        default: break;
      }
    } else if (*p >= 0x20 && *p < 0x7F) {
      out.push_back({(char)*p, false});
    }
  }
  return out;
}

const GFXglyph *glyph(const GFXfont *f, char c) {
  if ((uint8_t)c < f->first || (uint8_t)c > f->last) c = '?';
  return &f->glyph[(uint8_t)c - f->first];
}

int textWidth(const GFXfont *f, const char *s) {
  int w = 0;
  for (auto &g : decode(s)) w += glyph(f, g.c)->xAdvance;
  return w;
}

void text(const GFXfont *f, int x, int y, const char *s, Align a = LEFT, uint16_t color = BLACK) {
  if (a != LEFT) {
    int w = textWidth(f, s);
    x -= (a == CENTER) ? w / 2 : w;
  }
  canvas.setFont(f);
  int dot = f->yAdvance >= 50 ? 4 : f->yAdvance >= 35 ? 3 : 2;
  for (auto &it : decode(s)) {
    const GFXglyph *g = glyph(f, it.c);
    canvas.drawChar(x, y, it.c, color, color, 1);
    if (it.dots) {
      int top = y + g->yOffset;
      int dy = top - dot - (dot > 2 ? 2 : 1);
      int gx = x + g->xOffset;
      canvas.fillRect(gx + g->width * 22 / 100 - dot / 2, dy, dot, dot, color);
      canvas.fillRect(gx + g->width * 78 / 100 - dot / 2, dy, dot, dot, color);
    }
    x += g->xAdvance;
  }
}

void text(const GFXfont *f, int x, int y, const String &s, Align a = LEFT, uint16_t color = BLACK) {
  text(f, x, y, s.c_str(), a, color);
}

void hline(int y, int th = 2) { canvas.fillRect(0, y, EPD_W, th, BLACK); }

// ---------------------------------------------------------------- Icons
void drawBattery(int x, int y) {
  int pct = app::batteryPercent();
  canvas.drawRect(x, y, 24, 13, BLACK);
  canvas.drawRect(x + 1, y + 1, 22, 11, BLACK);
  canvas.fillRect(x + 24, y + 4, 3, 5, BLACK);
  int w = (20 * pct + 50) / 100;
  if (w > 0) canvas.fillRect(x + 2, y + 2, w, 9, BLACK);
  if (pct <= 10) {  // fast leer: Ausrufezeichen
    canvas.fillRect(x + 11, y + 3, 2, 5, BLACK);
    canvas.fillRect(x + 11, y + 9, 2, 2, BLACK);
  }
}

void drawWifi(int x, int y, bool connected) {
  // x/y = Mittelpunkt unten
  canvas.fillCircle(x, y, 2, BLACK);
  for (int r = 6; r <= 14; r += 4) {
    canvas.drawCircleHelper(x, y, r, 0x1 | 0x2, BLACK);
    canvas.drawCircleHelper(x, y, r - 1, 0x1 | 0x2, BLACK);
  }
  // nur obere Hälfte behalten
  canvas.fillRect(x - 16, y + 3, 33, 12, WHITE);
  if (!connected) {  // durchgestrichen = verbindet noch
    canvas.drawLine(x - 10, y - 12, x + 10, y + 2, BLACK);
    canvas.drawLine(x - 10, y - 11, x + 10, y + 3, BLACK);
  }
}

void drawTopBar() {
  String clock = timeutil::valid() ? timeutil::fmtClock(timeutil::now()) : String("--:--");
  text(F12, 3, 21, clock);
  int x = 168;
  drawBattery(x, 7);
  x -= 6;
  if (net::apOn()) {
    int w = textWidth(F9, "AP") + 8;
    x -= w;
    canvas.fillRoundRect(x, 3, w, 20, 4, BLACK);
    text(F9, x + 4, 19, "AP", LEFT, WHITE);
    x -= 6;
  }
  if (net::staActive()) {
    x -= 15;
    drawWifi(x, 20, net::staConnected());
  }
  hline(27);
}

// ------------------------------------------------------------- Ansichten
uint32_t nowEpoch() { return timeutil::now(); }

uint32_t workedToday(uint32_t now) {
  time_t d = timeutil::dayStart(now);
  return storage::workedBetween(d, timeutil::addDays(d, 1), now);
}
uint32_t workedWeek(uint32_t now) {
  time_t w = timeutil::weekStart(now);
  return storage::workedBetween(w, timeutil::addDays(w, 7), now);
}
uint32_t workedMonth(uint32_t now) {
  time_t m = timeutil::monthStart(now);
  return storage::workedBetween(m, timeutil::addMonths(m, 1), now);
}

void labelValue(int y, const char *label, const String &value) {
  text(F12, 4, y, label);
  text(F12, 196, y, value, RIGHT);
}

void drawMain() {
  drawTopBar();
  bool run = storage::running();
  if (run) {
    canvas.fillRoundRect(4, 33, 192, 40, 8, BLACK);
    text(F18, 100, 64, "L\xC3\x84UFT", CENTER, WHITE);
  } else {
    canvas.drawRoundRect(4, 33, 192, 40, 8, BLACK);
    canvas.drawRoundRect(5, 34, 190, 38, 7, BLACK);
    canvas.drawRoundRect(6, 35, 188, 36, 6, BLACK);
    text(F18, 100, 64, "PAUSE", CENTER);
  }

  if (!timeutil::valid()) {
    text(F12, 100, 104, "Uhrzeit fehlt!", CENTER);
    text(F12, 100, 132, "PWR 3x dr\xC3\xBC" "cken", CENTER);
    text(F12, 100, 160, "= WLAN-Sync", CENTER);
    text(F9, 100, 190, "oder Hotspot (5 s)", CENTER);
    return;
  }

  uint32_t now = nowEpoch();
  if (run) {
    uint32_t since = storage::runningSince();
    text(F24, 100, 113, timeutil::fmtDuration(now > since ? now - since : 0), CENTER);
    text(F12, 100, 136, "seit " + timeutil::fmtClock(since), CENTER);
    text(F9, 100, 152, "PWR = +1 Min", CENTER);
    hline(158);
    labelValue(178, "Heute", timeutil::fmtDuration(workedToday(now)));
    labelValue(198, "Woche", timeutil::fmtDuration(workedWeek(now)));
  } else {
    text(F12, 100, 98, "Heute", CENTER);
    text(F24, 100, 140, timeutil::fmtDuration(workedToday(now)), CENTER);
    hline(152);
    labelValue(176, "Woche", timeutil::fmtDuration(workedWeek(now)));
    labelValue(198, "Monat", timeutil::fmtDuration(workedMonth(now)));
  }
}

void drawNoTime() { text(F12, 100, 110, "Uhrzeit fehlt!", CENTER); }

void drawDay() {
  drawTopBar();
  if (!timeutil::valid()) return drawNoTime();
  uint32_t now = nowEpoch();
  struct tm lt = timeutil::local(now);
  char title[32];
  snprintf(title, sizeof(title), "Heute, %s %02d.%02d.", timeutil::weekdayShort(lt.tm_wday), lt.tm_mday, lt.tm_mon + 1);
  text(F12, 100, 50, title, CENTER);
  text(F24, 100, 94, timeutil::fmtDuration(workedToday(now)), CENTER);
  hline(104);

  time_t d0 = timeutil::dayStart(now), d1 = timeutil::addDays(d0, 1);
  std::vector<std::pair<uint32_t, uint32_t>> items;  // end 0 = läuft
  for (const auto &s : storage::sessions()) {
    if (s.end > (uint32_t)d0 && s.start < (uint32_t)d1) items.push_back({s.start, s.end});
  }
  if (storage::running()) items.push_back({storage::runningSince(), 0});
  if (items.empty()) {
    text(F12, 100, 150, "Noch keine", CENTER);
    text(F12, 100, 176, "Eintr\xC3\xA4ge", CENTER);
    return;
  }
  size_t maxLines = 4, first = 0;
  bool more = items.size() > maxLines;
  if (more) first = items.size() - (maxLines - 1);
  int y = 128;
  if (more) {
    char b[24];
    snprintf(b, sizeof(b), "+ %u weitere", (unsigned)first);
    text(F12, 100, y, b, CENTER);
    y += 24;
  }
  for (size_t i = first; i < items.size(); i++, y += 24) {
    String line = timeutil::fmtClock(items[i].first) + " - " +
                  (items[i].second ? timeutil::fmtClock(items[i].second) : String("l\xC3\xA4uft"));
    text(F12, 100, y, line, CENTER);
  }
}

void drawWeek() {
  drawTopBar();
  if (!timeutil::valid()) return drawNoTime();
  uint32_t now = nowEpoch();
  time_t w0 = timeutil::weekStart(now);
  char title[24];
  snprintf(title, sizeof(title), "Woche  KW %d", timeutil::isoWeek(now));
  text(F12, 100, 50, title, CENTER);
  time_t today = timeutil::dayStart(now);
  for (int i = 0; i < 7; i++) {
    time_t d = timeutil::addDays(w0, i);
    uint32_t sec = storage::workedBetween(d, timeutil::addDays(d, 1), now);
    int col = i < 4 ? 0 : 1, row = i < 4 ? i : i - 4;
    int x0 = col ? 102 : 0, y = 79 + row * 25;
    uint16_t color = BLACK;
    if (d == today) {
      canvas.fillRect(x0, y - 19, 98, 25, BLACK);
      color = WHITE;
    }
    text(F12, x0 + 4, y, timeutil::weekdayShort(timeutil::local(d).tm_wday), LEFT, color);
    text(F12, x0 + 94, y, sec ? timeutil::fmtDuration(sec) : String("-"), RIGHT, color);
  }
  canvas.fillRect(100, 58, 2, 98, BLACK);
  hline(160);
  text(F12, 4, 190, "Summe");
  text(F18, 196, 192, timeutil::fmtDuration(workedWeek(now)), RIGHT);
}

void drawMonth() {
  drawTopBar();
  if (!timeutil::valid()) return drawNoTime();
  uint32_t now = nowEpoch();
  struct tm lt = timeutil::local(now);
  char title[32];
  snprintf(title, sizeof(title), "%s %d", timeutil::monthName(lt.tm_mon + 1), lt.tm_year + 1900);
  text(F12, 100, 50, title, CENTER);
  uint32_t total = workedMonth(now);
  text(F24, 100, 96, timeutil::fmtDuration(total), CENTER);
  hline(106);

  time_t m0 = timeutil::monthStart(now), m1 = timeutil::addMonths(m0, 1);
  int days = 0;
  for (time_t d = m0; d < m1 && d <= (time_t)now; d = timeutil::addDays(d, 1)) {
    if (storage::workedBetween(d, timeutil::addDays(d, 1), now)) days++;
  }
  labelValue(134, "Arbeitstage", String(days));
  labelValue(162, "Schnitt/Tag", days ? timeutil::fmtDuration(total / days) : String("-"));
  time_t p0 = timeutil::addMonths(m0, -1);
  labelValue(190, "Vormonat", timeutil::fmtDuration(storage::workedBetween(p0, m0, now)));
}

void drawAp() {
  canvas.fillRect(0, 0, EPD_W, 26, BLACK);
  text(F12, 100, 20, "WLAN-Hotspot", CENTER, WHITE);

  static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
  static uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
  static bool qrOk = false;
  static bool qrDone = false;
  if (!qrDone) {
    String payload = String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASS + ";;";
    qrOk = qrcodegen_encodeText(payload.c_str(), tmp, qr, qrcodegen_Ecc_LOW, 1, 6, qrcodegen_Mask_AUTO, true);
    qrDone = true;
  }
  if (qrOk) {
    int n = qrcodegen_getSize(qr);
    int scale = 124 / n;
    if (scale > 4) scale = 4;
    int sz = n * scale;
    int x0 = (EPD_W - sz) / 2, y0 = 30 + (124 - sz) / 2;
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++)
        if (qrcodegen_getModule(qr, x, y)) canvas.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, BLACK);
  }
  text(F12, 100, 174, AP_SSID, CENTER);
  text(F12, 100, 197, String("PW: ") + AP_PASS, CENTER);
}

void drawMessage() {
  // Zeilen aufteilen
  std::vector<String> lines;
  int start = 0;
  while (true) {
    int nl = msgText.indexOf('\n', start);
    lines.push_back(nl < 0 ? msgText.substring(start) : msgText.substring(start, nl));
    if (nl < 0) break;
    start = nl + 1;
  }
  // pro Zeile die größte passende Schrift wählen
  const GFXfont *fonts[] = {F24, F18, F12, F9};
  std::vector<const GFXfont *> lf;
  int total = 0;
  for (auto &l : lines) {
    const GFXfont *f = F9;
    for (const GFXfont *cand : fonts) {
      if (textWidth(cand, l.c_str()) <= 190) {
        f = cand;
        break;
      }
    }
    lf.push_back(f);
    total += f->yAdvance * 9 / 10;
  }
  // zu hoch? alles eine Stufe kleiner
  while (total > 186) {
    total = 0;
    for (auto &f : lf) {
      if (f == F24) f = F18;
      else if (f == F18) f = F12;
      else f = F9;
      total += f->yAdvance * 9 / 10;
    }
    if (lf[0] == F9) break;
  }
  canvas.drawRoundRect(0, 0, EPD_W, EPD_H, 10, BLACK);
  canvas.drawRoundRect(1, 1, EPD_W - 2, EPD_H - 2, 9, BLACK);
  canvas.drawRoundRect(2, 2, EPD_W - 4, EPD_H - 4, 8, BLACK);
  // vertikal zentrieren (Oberkante erste Zeile bis Grundlinie letzte Zeile)
  int firstCap = -glyph(lf[0], 'A')->yOffset;
  int span = firstCap;
  for (size_t i = 1; i < lf.size(); i++) span += lf[i]->yAdvance * 9 / 10;
  int y = (EPD_H - span) / 2 + firstCap;
  for (size_t i = 0; i < lines.size(); i++) {
    if (i) y += lf[i]->yAdvance * 9 / 10;
    if (lines[i].length()) text(lf[i], 100, y, lines[i], CENTER);
  }
}

void render() {
  canvas.fillScreen(WHITE);
  if (msgActive) return drawMessage();
  switch (curView) {
    case V_MAIN: drawMain(); break;
    case V_DAY: drawDay(); break;
    case V_WEEK: drawWeek(); break;
    case V_MONTH: drawMonth(); break;
    case V_AP: drawAp(); break;
  }
}

void push(bool full) {
  const uint8_t *buf = canvas.getBuffer();
  if (!full && haveLastFrame && memcmp(buf, lastFrame, EPD_BUF_SIZE) == 0) return;
  if (full || !haveLastFrame || partialCount >= 60) {
    epd::fullRefresh(buf);
    partialCount = 0;
  } else {
    epd::partialRefresh(buf);
    partialCount++;
  }
  memcpy(lastFrame, buf, EPD_BUF_SIZE);
  haveLastFrame = true;
}

View homeView() { return net::apOn() ? V_AP : V_MAIN; }

}  // namespace

namespace ui {

void begin() {
  epd::begin();
  viewSince = millis();
}

void update() {
  uint32_t ms = millis();
  if (msgActive && !msgSticky && (int32_t)(ms - msgUntil) >= 0) {
    msgActive = false;
    redrawReq = true;
  }
  if (curView != homeView() && curView != V_MAIN && ms - viewSince > VIEW_TIMEOUT_MS) {
    curView = homeView();
    redrawReq = true;
  }
  if (curView == V_AP && !net::apOn()) {
    curView = V_MAIN;
    redrawReq = true;
  }
  // höchstens alle 2 s neu rendern (Inhalt ändert sich ohnehin nur minütlich)
  if (redrawReq || ms - lastRender > 2000) {
    redrawReq = false;
    lastRender = ms;
    render();
    push(false);
  }
}

void nextView() {
  switch (curView) {
    case V_MAIN: curView = V_DAY; break;
    case V_DAY: curView = V_WEEK; break;
    case V_WEEK: curView = V_MONTH; break;
    case V_MONTH: curView = net::apOn() ? V_AP : V_MAIN; break;
    case V_AP: curView = V_MAIN; break;
  }
  viewSince = millis();
  redrawReq = true;
}

void setView(View v) {
  curView = v;
  viewSince = millis();
  redrawReq = true;
}

View view() { return curView; }
void goHome() { setView(homeView()); }

void showMessage(const String &t, uint32_t ms) {
  msgText = t;
  msgActive = true;
  msgSticky = ms == 0;
  msgUntil = millis() + ms;
  redrawReq = true;
}

void clearMessage() {
  if (!msgActive) return;
  msgActive = false;
  redrawReq = true;
}

void requestRedraw() { redrawReq = true; }

void drawNow(bool full) {
  render();
  push(full);
  lastRender = millis();
  redrawReq = false;
}

}  // namespace ui
