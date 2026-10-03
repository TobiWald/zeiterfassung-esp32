// Hauptlogik: Tasten auswerten, Zeiterfassung steuern, Anzeige aktualisieren
#include "main_app.h"
#include "app.h"
#include "audio.h"
#include "config.h"
#include "epd.h"
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

// Stromsparen
uint32_t lastActivity = 0;  // letzte Aktivität (Taste, Webseite, Erfassung, Hotspot)
uint32_t apDeadline = 0;    // Hotspot-Ende (vor der Vorwarnung)
bool apWarning = false;     // Vorwarnung läuft
uint32_t apWarnUntil = 0;
int apWarnShown = -1;       // angezeigter Countdown-Wert

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

void startAp() {
  net::setAp(true);
  apDeadline = millis() + AP_TIMEOUT_MS;
  apWarning = false;
  ui::setView(V_AP);
}

void stopAp(const char *msg) {
  net::setAp(false);
  apWarning = false;
  ui::clearMessage();
  ui::showMessage(msg, 2500);
  ui::setView(V_MAIN);
  lastActivity = millis();  // ab jetzt läuft der Ausschalt-Timer
}

void extendAp() {
  apDeadline = millis() + AP_TIMEOUT_MS;
  apWarning = false;
  ui::clearMessage();
  ui::showMessage("Hotspot\n+2 Minuten", 2000);
}

// Ausschalten: im Akkubetrieb wird die Versorgung getrennt, an USB geht
// das Gerät in den Tiefschlaf. Einschalten/Aufwecken jeweils mit PWR.
void powerOffSequence(bool automatic) {
  net::setAp(false);
  net::setSta(false);
  hw::setLedBlink(false);
  ui::showMessage(automatic ? "AUS\n(Stromsparen)\n\nZum Einschalten\nPWR dr\xC3\xBC" "cken"
                            : "AUS\n\nZum Einschalten\nPWR dr\xC3\xBC" "cken",
                  0);
  ui::drawNow(true);
  epd::sleep();
  delay(300);
  hw::powerOff();
  delay(1500);
  // Läuft noch -> USB-Versorgung: Tiefschlaf
  hw::deepSleep();
}

void powerManagement() {
  uint32_t ms = millis();

  // Hotspot: nach 2 Minuten Vorwarnung, 15 s später aus
  if (net::apOn()) {
    if (!apWarning && (int32_t)(ms - apDeadline) >= 0) {
      apWarning = true;
      apWarnUntil = ms + AP_WARN_MS;
      apWarnShown = -1;
      audio::beep(3);
    }
    if (apWarning) {
      int left = ((int32_t)(apWarnUntil - ms) + 999) / 1000;
      if (left <= 0) {
        stopAp("Hotspot AUS");
      } else if (left != apWarnShown && !hw::menuButtonDown()) {
        apWarnShown = left;
        ui::showMessage("Hotspot aus\nin " + String(left) + " s\n\nPWR kurz dr\xC3\xBC" "cken\n= +2 Minuten", 0);
      }
    }
  }

  // Ausschalten nach Inaktivität – nicht während Erfassung, Hotspot,
  // Uhrzeit-Abgleich oder gedrückter Taste
  if (storage::running() || net::apOn() || net::timeSyncPending() || hw::menuButtonDown()) {
    lastActivity = ms;
  } else if (ms - lastActivity >= AUTO_OFF_MS) {
    powerOffSequence(true);
  }
}

void onEvent(const BtnEvent &e) {
  lastActivity = millis();
  // Während der Hotspot-Vorwarnung verlängert ein kurzer PWR-Druck
  if (apWarning && e.btn == BTN_MENU) {
    if (e.type == EV_CLICKS) return extendAp();
    if (e.type == EV_HOLD_REACHED) {  // Halten: Warnung beenden, normal weiter
      apDeadline = millis() + AP_TIMEOUT_MS;
      apWarning = false;
    }
  }
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
        powerOffSequence(false);
      } else if (net::apOn()) {
        stopAp("Hotspot AUS");
      } else {
        startAp();
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
void noteActivity() { lastActivity = millis(); }
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
  lastActivity = millis();
}

void appLoop() {
  BtnEvent e;
  while (hw::nextEvent(&e)) onEvent(e);

  net::loop();
  String n = net::takeNotice();
  if (n.length()) ui::showMessage(n, 4000);

  if (millis() - lastBatRead > 30000) readBattery();
  dailyTimeSync();
  powerManagement();
  ui::update();
  delay(10);
}
