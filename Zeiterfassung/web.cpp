#include "web.h"
#include <WebServer.h>
#include <WiFi.h>
#include "app.h"
#include "config.h"
#include "exporter.h"
#include "net.h"
#include "page.h"
#include "storage.h"
#include "timeutil.h"

namespace {

WebServer server(80);

bool isIp(const String &s) {
  for (char c : s) {
    if (c != '.' && c != ':' && (c < '0' || c > '9')) return false;
  }
  return s.length() > 0;
}

// Captive Portal: fremde Hostnamen im AP-Modus auf unsere Seite umleiten
bool captiveRedirect() {
  if (!net::apOn()) return false;
  String host = server.hostHeader();
  int colon = host.indexOf(':');
  if (colon >= 0) host = host.substring(0, colon);
  if (isIp(host)) return false;
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
  return true;
}

// String als JSON-Literal (mit Anführungszeichen) anhängen
void jsonStr(String &j, const String &v) {
  j += '"';
  for (char c : v) {
    if (c == '"' || c == '\\') {
      j += '\\';
      j += c;
    } else if ((uint8_t)c < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", (uint8_t)c);
      j += b;
    } else {
      j += c;
    }
  }
  j += '"';
}

void sendJson(const String &s) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

void handleRoot() {
  if (captiveRedirect()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
}

void handleStatus() {
  uint32_t now = timeutil::now();
  bool valid = timeutil::valid();
  String j;
  j.reserve(320);
  j += "{\"valid\":"; j += valid ? "true" : "false";
  j += ",\"now\":"; j += now;
  j += ",\"running\":"; j += storage::running() ? "true" : "false";
  j += ",\"since\":"; j += storage::runningSince();
  if (valid) {
    j += ",\"today\":"; j += storage::workedBetween(timeutil::dayStart(now), timeutil::addDays(timeutil::dayStart(now), 1), now);
    j += ",\"week\":"; j += storage::workedBetween(timeutil::weekStart(now), timeutil::addDays(timeutil::weekStart(now), 7), now);
    j += ",\"month\":"; j += storage::workedBetween(timeutil::monthStart(now), timeutil::addMonths(timeutil::monthStart(now), 1), now);
  }
  j += ",\"bat\":"; j += app::batteryPercent();
  j += ",\"batV\":"; j += String(app::batteryVoltage(), 2);
  j += ",\"sta\":"; j += net::staConnected() ? "true" : "false";
  j += ",\"staIp\":\""; j += net::staConnected() ? net::staIp() : String(""); j += "\"";
  j += ",\"ssid\":"; jsonStr(j, net::currentSsid());
  j += ",\"ap\":"; j += net::apOn() ? "true" : "false";
  j += ",\"lastSync\":"; j += net::lastSyncEpoch();
  j += ",\"count\":"; j += (uint32_t)storage::sessions().size();
  j += "}";
  sendJson(j);
}

void handleMonth() {
  uint32_t now = timeutil::now();
  struct tm lt = timeutil::local(now);
  int y = server.hasArg("y") ? server.arg("y").toInt() : lt.tm_year + 1900;
  int m = server.hasArg("m") ? server.arg("m").toInt() : lt.tm_mon + 1;
  if (m < 1 || m > 12 || y < 2000 || y > 2100) {
    server.send(400, "text/plain", "bad month");
    return;
  }
  time_t from = timeutil::monthStart(y, m);
  time_t to = timeutil::addMonths(from, 1);

  String j;
  j.reserve(4096);
  j += "{\"y\":"; j += y; j += ",\"m\":"; j += m;
  j += ",\"total\":"; j += storage::workedBetween(from, to, now);
  j += ",\"days\":[";
  bool first = true;
  for (time_t d = from; d < to; d = timeutil::addDays(d, 1)) {
    if (!first) j += ",";
    first = false;
    j += storage::workedBetween(d, timeutil::addDays(d, 1), now);
  }
  j += "],\"weeks\":[";
  first = true;
  for (time_t w = timeutil::weekStart(from); w < to; w = timeutil::addDays(w, 7)) {
    if (!first) j += ",";
    first = false;
    j += "{\"kw\":"; j += timeutil::isoWeek(w);
    j += ",\"from\":"; j += (uint32_t)w;
    j += ",\"sec\":"; j += storage::workedBetween(w, timeutil::addDays(w, 7), now);
    j += "}";
  }
  j += "],\"sessions\":[";
  first = true;
  for (const auto &s : storage::sessions()) {
    if (s.start < from || s.start >= (uint32_t)to) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"s\":"; j += s.start; j += ",\"e\":"; j += s.end; j += "}";
  }
  if (storage::running() && storage::runningSince() >= from && storage::runningSince() < (uint32_t)to) {
    if (!first) j += ",";
    j += "{\"s\":"; j += storage::runningSince(); j += ",\"e\":0,\"run\":1}";
  }
  j += "]}";
  sendJson(j);
}

// Die letzten n Einträge (neueste zuerst, inkl. laufender Erfassung)
void handleRecent() {
  int n = server.hasArg("n") ? server.arg("n").toInt() : 5;
  if (n < 1 || n > 50) n = 5;
  String j = "{\"sessions\":[";
  bool first = true;
  if (storage::running()) {
    j += "{\"s\":"; j += storage::runningSince(); j += ",\"e\":0,\"run\":1}";
    first = false;
    n--;
  }
  const auto &list = storage::sessions();
  for (int i = (int)list.size() - 1; i >= 0 && n > 0; i--, n--) {
    if (!first) j += ",";
    first = false;
    j += "{\"s\":"; j += list[i].start; j += ",\"e\":"; j += list[i].end; j += "}";
  }
  j += "]}";
  sendJson(j);
}

bool exportRange(exporter::Range *r) {
  uint32_t now = timeutil::now();
  if (server.hasArg("y") && server.hasArg("m")) {
    int y = server.arg("y").toInt(), m = server.arg("m").toInt();
    if (m < 1 || m > 12 || y < 2000 || y > 2100) return false;
    *r = exporter::rangeForMonth(y, m);
  } else {
    *r = exporter::rangeAll(now);
  }
  return true;
}

void handleXlsx() {
  exporter::Range r;
  if (!exportRange(&r)) return server.send(400, "text/plain", "bad range");
  exporter::sendXlsx(server, r, timeutil::now());
}

void handleCsv() {
  exporter::Range r;
  if (!exportRange(&r)) return server.send(400, "text/plain", "bad range");
  exporter::sendCsv(server, r, timeutil::now());
}

void handleToggle() {
  if (!timeutil::valid()) return server.send(409, "text/plain", "Uhrzeit nicht gesetzt");
  app::toggleTracking();
  handleStatus();
}

void handleSetTime() {
  uint32_t t = strtoul(server.arg("t").c_str(), nullptr, 10);
  if (t < 1735689600UL) return server.send(400, "text/plain", "bad time");
  timeutil::set(t);
  app::requestRedraw();
  handleStatus();
}

void handleDelete() {
  uint32_t s = strtoul(server.arg("s").c_str(), nullptr, 10);
  if (!storage::deleteSession(s)) return server.send(404, "text/plain", "nicht gefunden");
  app::requestRedraw();
  sendJson("{\"ok\":true}");
}

void handleAdd() {
  uint32_t s = strtoul(server.arg("s").c_str(), nullptr, 10);
  uint32_t e = strtoul(server.arg("e").c_str(), nullptr, 10);
  if (s < 1735689600UL || e <= s || e - s > 24 * 3600) return server.send(400, "text/plain", "ungültig");
  if (!storage::addSession(s, e)) return server.send(500, "text/plain", "Speicherfehler");
  app::requestRedraw();
  sendJson("{\"ok\":true}");
}

void handleReset() {
  if (server.arg("confirm") != "1") return server.send(400, "text/plain", "Best\xC3\xA4tigung fehlt");
  if (!storage::clearAll()) return server.send(500, "text/plain", "Speicherfehler");
  app::dataReset();
  sendJson("{\"ok\":true}");
}

void handleWifi() {
  String j = "{\"current\":";
  jsonStr(j, net::currentSsid());
  j += ",\"builtin\":";
  jsonStr(j, WIFI_SSID);
  j += ",\"saved\":[";
  bool first = true;
  for (auto &w : net::savedNetworks()) {
    if (!first) j += ",";
    first = false;
    jsonStr(j, w.ssid);
  }
  j += "]}";
  sendJson(j);
}

void handleWifiScan() {
  if (net::scanStart() < 0) return sendJson("{\"scanning\":true}");
  auto saved = net::savedNetworks();
  String j = "{\"scanning\":false,\"nets\":[";
  bool first = true;
  for (auto &r : net::scanResults()) {
    bool known = r.ssid == WIFI_SSID;
    for (auto &w : saved) known |= w.ssid == r.ssid;
    if (!first) j += ",";
    first = false;
    j += "{\"ssid\":";
    jsonStr(j, r.ssid);
    j += ",\"rssi\":"; j += r.rssi;
    j += ",\"secure\":"; j += r.secure ? "true" : "false";
    j += ",\"known\":"; j += known ? "true" : "false";
    j += "}";
  }
  j += "]}";
  sendJson(j);
}

void handleWifiAdd() {
  if (!net::addNetwork(server.arg("ssid"), server.arg("pass")))
    return server.send(400, "text/plain", "Ung\xC3\xBCltiger Netzname oder Passwort");
  sendJson("{\"ok\":true}");
}

void handleWifiDel() {
  if (!net::removeNetwork(server.arg("ssid"))) return server.send(404, "text/plain", "nicht gefunden");
  sendJson("{\"ok\":true}");
}

void handleNotFound() {
  if (captiveRedirect()) return;
  server.send(404, "text/plain", "Nicht gefunden");
}

}  // namespace

namespace web {

void begin() {
  // jede Anfrage zählt als Aktivität (Stromsparen)
  auto on = [](const char *path, HTTPMethod m, void (*fn)()) {
    server.on(path, m, [fn]() {
      app::noteActivity();
      fn();
    });
  };
  on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);  // Hintergrund-Abfrage: keine Aktivität
  on("/api/month", HTTP_GET, handleMonth);
  server.on("/api/recent", HTTP_GET, handleRecent);  // wird regelmäßig abgefragt: keine Aktivität
  on("/api/toggle", HTTP_POST, handleToggle);
  on("/api/settime", HTTP_POST, handleSetTime);
  on("/api/delete", HTTP_POST, handleDelete);
  on("/api/add", HTTP_POST, handleAdd);
  on("/api/reset", HTTP_POST, handleReset);
  on("/api/wifi", HTTP_GET, handleWifi);
  on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  on("/api/wifi/add", HTTP_POST, handleWifiAdd);
  on("/api/wifi/del", HTTP_POST, handleWifiDel);
  on("/export.xlsx", HTTP_GET, handleXlsx);
  on("/export.csv", HTTP_GET, handleCsv);
  // Erkennungs-URLs von Android/iOS/Windows -> Portal öffnen
  on("/generate_204", HTTP_ANY, handleRoot);
  on("/gen_204", HTTP_ANY, handleRoot);
  on("/hotspot-detect.html", HTTP_ANY, handleRoot);
  on("/connecttest.txt", HTTP_ANY, handleRoot);
  on("/ncsi.txt", HTTP_ANY, handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() { server.handleClient(); }

}  // namespace web
