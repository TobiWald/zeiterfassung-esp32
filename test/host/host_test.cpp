// Host-Test: rendert alle Display-Ansichten als PNG und erzeugt Export-Dateien.
#include <Arduino.h>
#include <fstream>
#include "../../Zeiterfassung/timeutil.h"
#include "../../Zeiterfassung/storage.h"
#include "../../Zeiterfassung/ui.h"
#include "../../Zeiterfassung/exporter.h"

extern uint32_t g_millis; extern bool g_ap, g_sta, g_staConn; extern int g_bat;
namespace epd { extern uint8_t shown[5000]; }

static void dump(const char *name) {
  std::ofstream f(std::string("out/") + name + ".pbm", std::ios::binary);
  f << "P4\n200 200\n";
  for (int i = 0; i < 5000; i++) f.put((char)~epd::shown[i]);  // PBM: 1 = schwarz
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();
  time_t now = time(nullptr);
  time_t today = timeutil::dayStart(now);
  // Beispieldaten: 45 Tage zurück, werktags 2 Blöcke
  for (int d = 45; d >= 1; d--) {
    time_t day = timeutil::addDays(today, -d);
    int wd = timeutil::local(day).tm_wday;
    if (wd == 0 || wd == 6) continue;
    storage::addSession(day + 8 * 3600 + 5 * 60, day + 12 * 3600 + 30 * 60);
    storage::addSession(day + 13 * 3600, day + 16 * 3600 + 47 * 60);
  }
  storage::addSession(today + 7 * 3600 + 58 * 60, today + 11 * 3600 + 2 * 60);
  if (now - today > 12 * 3600) storage::start(today + 12 * 3600);
  else storage::start(now - 2 * 3600 - 17 * 60);

  ui::begin();
  g_sta = true; g_staConn = true;
  ui::drawNow(true); dump("1_main_running");
  ui::setView(V_DAY); ui::drawNow(); dump("2_day");
  ui::setView(V_WEEK); ui::drawNow(); dump("3_week");
  ui::setView(V_MONTH); ui::drawNow(); dump("4_month");
  g_ap = true; g_bat = 8;
  ui::setView(V_AP); ui::drawNow(); dump("5_ap");
  storage::stop(now);
  ui::setView(V_MAIN); ui::drawNow(); dump("6_main_paused");
  ui::showMessage("GESTARTET\n08:15"); ui::drawNow(); dump("7_msg_start");
  ui::showMessage("Loslassen:\nHotspot AN"); ui::drawNow(); dump("8_msg_hold");
  ui::showMessage("Hole Uhrzeit\n\xC3\xBC" "ber WLAN..."); ui::drawNow(); dump("9_msg_sync");
  ui::showMessage("GESTOPPT\n5:34\nHeute 8:38"); ui::drawNow(); dump("9b_msg_stop");
  ui::showMessage("AUS\n\nZum Einschalten\nPWR dr\xC3\xBC" "cken"); ui::drawNow(); dump("9c_msg_off");
  ui::showMessage("WLAN verbunden\n192.168.178.57"); ui::drawNow(); dump("9d_msg_wlan");
  ui::clearMessage();

  struct tm lt = timeutil::local(now);
  WebServer a; exporter::sendXlsx(a, exporter::rangeForMonth(lt.tm_year + 1900, lt.tm_mon + 1), now);
  std::ofstream("out/month.xlsx", std::ios::binary) << a.body;
  printf("xlsx month: declared %zu, body %zu\n", a.declared, a.body.size());
  WebServer b; exporter::sendXlsx(b, exporter::rangeAll(now), now);
  std::ofstream("out/all.xlsx", std::ios::binary) << b.body;
  printf("xlsx all: declared %zu, body %zu\n", b.declared, b.body.size());
  WebServer c; exporter::sendCsv(c, exporter::rangeAll(now), now);
  std::ofstream("out/all.csv", std::ios::binary) << c.body;
  printf("csv: declared %zu, body %zu\n", c.declared, c.body.size());
  return 0;
}
