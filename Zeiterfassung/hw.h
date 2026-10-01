#pragma once
#include <Arduino.h>

enum BtnId : uint8_t { BTN_TRACK = 0, BTN_MENU = 1 };
enum BtnEvType : uint8_t {
  EV_CLICKS,         // value = Anzahl Klicks
  EV_HOLD_REACHED,   // value = erreichte Haltezeit in ms (5000 / 10000)
  EV_HOLD_RELEASED,  // value = Haltedauer in ms
};

struct BtnEvent {
  BtnId btn;
  BtnEvType type;
  uint32_t value;
};

namespace hw {
void earlyInit();             // Akku-Selbsthaltung, Audio aus, LED aus
void begin();                 // Tasten-/LED-Task starten
bool nextEvent(BtnEvent *ev); // nicht blockierend
void setLedBlink(bool on);
void ledFlash(int times);     // kurze Bestätigung
bool menuButtonDown();

float batteryVoltage();
int batteryPercent(float v);
void powerOff();              // trennt Akku (USB-Betrieb läuft weiter)
void powerHold();             // Akku-Selbsthaltung wieder aktivieren
}
