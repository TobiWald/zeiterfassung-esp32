#pragma once
#include <Arduino.h>

// Funktionen aus der Hauptlogik, die vom Webserver genutzt werden
namespace app {
void toggleTracking();     // starten / stoppen
void requestRedraw();      // Display bei nächster Gelegenheit neu zeichnen
void dataReset();          // nach dem Löschen aller Daten
void noteActivity();       // Benutzeraktivität (verzögert das Ausschalten)
float batteryVoltage();
int batteryPercent();
}
