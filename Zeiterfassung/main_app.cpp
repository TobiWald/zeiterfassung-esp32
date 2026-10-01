// Hauptlogik: Tasten auswerten, Zeiterfassung steuern, Anzeige aktualisieren
#include "main_app.h"
#include "app.h"
#include "config.h"
#include "hw.h"
#include "net.h"
#include "storage.h"
#include "timeutil.h"
#include "ui.h"

namespace {

float batV = 0;
int batPct = 0;
uint32_t lastBatRead = 0;
int lastDailySyncDay = -1;

void readBattery() {
  batV = hw::batteryVoltage();
  batPct = hw::batteryPercent(batV);
  lastBatRead = millis();
}

uint32_t workedToday(uint32_t now) {
  time_t d = timeutil::dayStart(now);
  return storage::workedBetween(d, timeutil::addDays(d, 1), now);
}

void toggle(bool fromWeb) {
  uint32_t now = timeutil::now();
  if (storage::running()) {
    uint32_t dur = now - storage::runningSince();
    storage::stop(now);
    ui::showMessage("GESTOPPT\n" + timeutil::fmtDuration(dur) + "\nHeute " + timeutil::fmtDuration(workedToday(now)),
                    4000);
    hw::ledFlash(1);
  } else {
    storage::start(now);
    ui::showMessage("GESTARTET\n" + timeutil::fmtClock(now), 3000);
  }
  hw::setLedBlink(storage::running());
  if (!fromWeb) ui::setView(V_MAIN);
}

void onTrackDoubleClick() {
  if (!timeutil::valid()) {
    ui::showMessage("Hole Uhrzeit\n\xC3\xBC" "ber WLAN...", 0);
    ui::drawNow();
    bool ok = net::syncTimeBlocking(30000);
    net::takeNotice();
    ui::clearMessage();
    if (!ok || !timeutil::valid()) {
      ui::showMessage("Keine Uhrzeit!\nWLAN fehlt.\nHotspot nutzen", 6000);
      return;
    }
  }
  toggle(false);
}

void powerOffSequence() {
  net::setAp(false);
  net::setSta(false);
  ui::showMessage("AUS\n\nZum Einschalten\nPWR dr\xC3\xBC" "cken", 0);
  ui::drawNow(true);
  delay(300);
  hw::powerOff();
  delay(2500);
  // Läuft noch -> USB-Versorgung
  hw::powerHold();
  ui::showMessage("USB-Betrieb\nGer\xC3\xA4t bleibt an", 3000);
}

void onEvent(const BtnEvent &e) {
  if (e.btn == BTN_TRACK) {
    if (e.type == EV_CLICKS && e.value == 2) onTrackDoubleClick();
    else if (e.type == EV_CLICKS && e.value == 1) {
      ui::clearMessage();
      ui::goHome();
    }
    return;
  }
  // PWR-Taste
  switch (e.type) {
    case EV_CLICKS:
      if (e.value == 1) {
        ui::clearMessage();
        ui::nextView();
      } else if (e.value == 3) {
        bool on = !net::staWanted();
        net::setSta(on);
        ui::showMessage(on ? "WLAN AN\nverbinde..." : "WLAN AUS", 3000);
      }
      break;
    case EV_HOLD_REACHED:
      if (e.value >= POWEROFF_HOLD_MS) ui::showMessage("Loslassen:\nAUSSCHALTEN", 0);
      else ui::showMessage(net::apOn() ? "Loslassen:\nHotspot AUS" : "Loslassen:\nHotspot AN", 0);
      ui::drawNow();
      break;
    case EV_HOLD_RELEASED:
      ui::clearMessage();
      if (e.value >= POWEROFF_HOLD_MS) {
        powerOffSequence();
      } else if (net::apOn()) {
        net::setAp(false);
        ui::showMessage("Hotspot AUS", 2500);
        ui::setView(V_MAIN);
      } else {
        net::setAp(true);
        ui::setView(V_AP);
      }
      break;
  }
}

void dailyTimeSync() {
  if (!timeutil::valid()) return;
  struct tm lt = timeutil::local(timeutil::now());
  if (lt.tm_hour == 3 && lt.tm_min >= 30 && lastDailySyncDay != lt.tm_yday && !net::timeSyncPending()) {
    lastDailySyncDay = lt.tm_yday;
    net::requestTimeSync();
  }
}

}  // namespace

namespace app {
void toggleTracking() { toggle(true); }
void requestRedraw() { ui::requestRedraw(); }
void dataReset() {
  hw::setLedBlink(false);
  ui::showMessage("Alle Daten\ngel\xC3\xB6scht", 4000);
}
float batteryVoltage() { return batV; }
int batteryPercent() { return batPct; }
}

void appSetup() {
  hw::earlyInit();
  Serial.begin(115200);
  timeutil::begin();
  storage::begin();
  net::begin();
  readBattery();
  ui::begin();
  ui::drawNow(true);
  hw::begin();
  hw::setLedBlink(storage::running());
  if (!timeutil::valid()) net::requestTimeSync();
}

void appLoop() {
  BtnEvent e;
  while (hw::nextEvent(&e)) onEvent(e);

  net::loop();
  String n = net::takeNotice();
  if (n.length()) ui::showMessage(n, 4000);

  if (millis() - lastBatRead > 30000) readBattery();
  dailyTimeSync();
  ui::update();
  delay(10);
}
