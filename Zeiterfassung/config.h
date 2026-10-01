#pragma once
// =====================================================================
//  Zeiterfassung – Konfiguration
//  Hardware: Waveshare ESP32-S3-ePaper-1.54 (V2)
// =====================================================================

// WLAN-Zugangsdaten für das Heimnetz (nur für Uhrzeit per NTP).
// Die Datei secrets.h wird NICHT ins Git eingecheckt – siehe secrets.example.h
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Bitte secrets.example.h nach secrets.h kopieren und WLAN-Daten eintragen."
#endif

// ---- Eigener Access Point (für Übersicht / Excel-Download) ----------
#ifndef AP_SSID
#define AP_SSID "Zeiterfassung"
#endif
#ifndef AP_PASS
#define AP_PASS "zeit1234"   // mind. 8 Zeichen
#endif

// ---- Zeitzone (Deutschland, inkl. Sommerzeit) -----------------------
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER_1 "fritz.box"
#define NTP_SERVER_2 "pool.ntp.org"
#define NTP_SERVER_3 "time.google.com"

// ---- Tasten ----------------------------------------------------------
// BOOT-Taste (GPIO0)  : 2x schnell = Arbeitszeit starten / stoppen
// PWR-Taste  (GPIO18) : 1x = Ansicht wechseln (Status/Tag/Woche/Monat)
//                       3x = Heim-WLAN an/aus (Uhrzeit holen)
//                       5 s halten  = Access Point an/aus
//                       10 s halten = Ausschalten (nur im Akkubetrieb)
#define PIN_BTN_TRACK 0
#define PIN_BTN_MENU  18

#define CLICK_GAP_MS     400   // max. Pause zwischen zwei Klicks
#define AP_HOLD_MS       5000
#define POWEROFF_HOLD_MS 10000
#define VIEW_TIMEOUT_MS  60000 // zurück zur Hauptansicht

// ---- Board-Pins (aus Waveshare V2 Beispielcode) ----------------------
#define PIN_EPD_DC   10
#define PIN_EPD_CS   11
#define PIN_EPD_SCK  12
#define PIN_EPD_MOSI 13
#define PIN_EPD_RST  9
#define PIN_EPD_BUSY 8
#define PIN_EPD_PWR  6   // LOW = Display an

#define PIN_VBAT_HOLD 17 // HIGH = Akku-Versorgung halten
#define PIN_AUDIO_PWR 42 // HIGH = Audio aus
#define PIN_LED       3  // grüne LED, LOW = an
#define PIN_BAT_ADC   4  // Akku-Spannung / 2

#define PIN_I2C_SDA 47
#define PIN_I2C_SCL 48
#define RTC_I2C_ADDR 0x51 // PCF85063

// LED-Blinkmuster während die Zeiterfassung läuft
#define LED_ON_MS     150
#define LED_PERIOD_MS 1000
