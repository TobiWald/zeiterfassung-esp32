# Zeiterfassung – Waveshare ESP32-S3-ePaper-1.54 (V2)

Kleines Gerät zur Arbeitszeiterfassung mit 1,54"-E-Paper-Display, zwei Tasten,
LED, Akkuanzeige, Uhrzeit per WLAN (NTP) und eigenem WLAN-Hotspot mit
Übersichtsseite und Excel-Download.

## Bedienung

| Taste | Aktion | Funktion |
|---|---|---|
| **BOOT** | 2× schnell drücken | Arbeitszeit **starten / stoppen** |
| BOOT | 1× drücken | zurück zur Hauptansicht |
| **PWR** | 1× drücken | Ansicht wechseln: Status → Tag → Woche → Monat (→ Hotspot) |
| PWR | 3× schnell drücken | Heim-WLAN **an/aus** (holt Datum und Uhrzeit) |
| PWR | 5 s halten, loslassen | eigenen **Hotspot an/aus** (Übersicht + Excel-Download) |
| PWR | 10 s halten, loslassen | **Ausschalten** (an USB: Tiefschlaf) |

Beim Halten zeigt das Display an, was beim Loslassen passiert.
Nach 60 s springt die Anzeige automatisch zur Hauptansicht zurück.

**LED:** blinkt, solange die Arbeitszeit läuft; aus, wenn pausiert.

**Display:** Uhrzeit, WLAN-Symbol (durchgestrichen = verbindet noch),
„AP“ = Hotspot aktiv, Akkusymbol (mit „!“ unter 10 %).

### Stromsparen

* **Automatisch aus nach 3 Minuten ohne Aktivität** – sofern keine Arbeitszeit
  läuft und der Hotspot aus ist. Als Aktivität zählen Tastendrücke und Aktionen
  auf der Webseite. Auf dem Display steht danach „AUS (Stromsparen)“.
  **Einschalten mit der PWR-Taste.**
* Im Akkubetrieb wird die Versorgung komplett getrennt. An USB geht das Gerät
  in den Tiefschlaf (PWR weckt es auf).
* **Hotspot endet nach 2 Minuten:** Es piept dreimal und das Display zählt
  15 Sekunden herunter. **PWR kurz drücken = +2 Minuten.** Erst wenn der Hotspot
  aus ist, beginnt der 3-Minuten-Timer zum Ausschalten.
* Mit eingestecktem Akku läuft die eingebaute Uhr (RTC) auch im ausgeschalteten
  Zustand weiter.

### Uhrzeit

* Die Uhrzeit wird in der eingebauten RTC (PCF85063) gespeichert.
* Fehlt sie (z. B. nach komplettem Stromverlust), verbindet sich das Gerät
  beim Start automatisch mit dem Heim-WLAN und holt sie per NTP.
* Wird ohne gültige Uhrzeit gestartet (BOOT 2×), versucht das Gerät zuerst
  die Uhrzeit zu holen.
* Jede Nacht um 3:30 Uhr wird die Uhrzeit kurz per WLAN nachgestellt.
* Alternativ: Hotspot einschalten und mit dem Handy verbinden – die Webseite
  übernimmt automatisch die Uhrzeit des Handys, wenn das Gerät keine hat.

### Hotspot / Übersicht / Excel

1. PWR 5 s halten → Display zeigt QR-Code, Netzname und Passwort
   (Standard: `Zeiterfassung` / `zeit1234`).
2. QR-Code mit dem Handy scannen oder manuell verbinden.
   Die Übersichtsseite öffnet sich automatisch (Captive Portal).
3. Falls Downloads im automatisch geöffneten Fenster nicht gehen:
   im Browser `http://192.168.4.1` öffnen.

Ist das Heim-WLAN eingeschaltet (PWR 3×), ist dieselbe Seite auch im
Heimnetz unter der auf dem Display angezeigten IP erreichbar.

Die Seite zeigt: Status (mit Start/Stopp-Knopf), Heute/Woche/Monat,
**Letzte Einträge** (die 5 neuesten, mit ✕ löschbar; aktualisiert sich
automatisch, auch wenn am Gerät gestartet/gestoppt wird),
Monatsübersicht mit Wochen und Tagen, alle Einträge (rotes ✕ hinter dem
Eintrag → Abfrage „Sicher löschen?“ mit Datum und Uhrzeit, Ja/Nein),
„Eintrag nachtragen“ und Downloads:

* **Excel – Monat** / **Excel – alles** (`.xlsx` mit den Blättern
  *Einträge*, *Tage*, *Wochen*, *Monate* inkl. Summen)
* **CSV – Monat** (Semikolon, deutsches Zahlenformat)

**Datum & Uhrzeit:** zeigt die Uhr des Geräts. Falls kein WLAN erreichbar
ist, lassen sich Datum und Uhrzeit hier von Hand einstellen oder mit einem
Klick vom Handy übernehmen.

**WLAN für Datum & Uhrzeit:** „Netzwerke in der Nähe suchen“ zeigt die
WLANs mit Signalstärke an; antippen, Passwort eingeben, „Speichern & verbinden“.
Das Gerät verbindet sich sofort und holt die Uhrzeit. Bis zu 5 Netze werden
gespeichert (mit ✕ entfernbar); das Gerät nimmt immer das stärkste bekannte
Netz in Reichweite. Das fest eingebaute WLAN aus `secrets.h` bleibt als
Rückfall erhalten.

Ganz unten: **Alle Daten löschen** (Reset). Löscht alle Einträge und beendet
eine laufende Erfassung – mit doppelter Sicherheitsabfrage (Eingabe von
„LÖSCHEN“). Vorher am besten „Excel – alles“ herunterladen.

## Flashen der fertigen Firmware

Die kombinierte Datei `zeiterfassung_combined_0x0.bin` enthält Bootloader,
Partitionstabelle und Programm und wird an **Adresse 0x0** geschrieben:

```bash
pip install esptool
# beim ersten Mal empfohlen (löscht auch alte Daten der Werks-Firmware):
esptool --chip esp32s3 erase-flash
esptool --chip esp32s3 write-flash 0x0 zeiterfassung_combined_0x0.bin
```

Oder im Browser mit dem [ESP Web Flasher](https://espressif.github.io/esptool-js/)
(Adresse `0x0`). Falls das Board nicht erkannt wird: BOOT gedrückt halten,
USB einstecken, BOOT loslassen.

> Die erfassten Zeiten liegen in einer eigenen Flash-Partition und bleiben bei
> späteren Updates erhalten – solange nicht `erase-flash` ausgeführt wird.

## Selbst bauen

1. `Zeiterfassung/secrets.example.h` nach `Zeiterfassung/secrets.h` kopieren
   und die WLAN-Daten eintragen (die Datei wird nicht eingecheckt).
2. Arduino-ESP32-Core **3.0.x** und die Bibliothek **Adafruit GFX Library**
   installieren.
3. Bauen:
   ```bash
   tools/build.sh      # erzeugt release/zeiterfassung_combined_0x0.bin
   ```
   Oder in der Arduino IDE: Board *ESP32S3 Dev Module*, USB CDC On Boot
   *Enabled*, Flash Size *4MB*, Partition Scheme *Huge APP (3MB No OTA/1MB
   SPIFFS)*, PSRAM *Disabled*.

Einstellungen (Tasten, Zeiten, Hotspot-Name/-Passwort, Zeitzone) stehen in
`Zeiterfassung/config.h`.

### Host-Test

`test/host/build.sh` kompiliert Anzeige und Export auf dem PC, rendert alle
Display-Ansichten als Bilder (`test/host/out/*.pbm`) und erzeugt Beispiel-
Excel-/CSV-Dateien.

## Aufbau

| Datei | Inhalt |
|---|---|
| `main_app.cpp` | Tastenlogik, Starten/Stoppen, Ausschalten |
| `hw.cpp` | Tasten (Klick/Mehrfachklick/Halten), LED, Akku, Selbsthaltung |
| `epd.cpp` | E-Paper-Treiber (SSD1681, Sequenzen aus dem Waveshare-Beispiel) |
| `ui.cpp` | Display-Ansichten, große Schrift, Umlaute, QR-Code |
| `timeutil.cpp` | RTC PCF85063, Zeitzone, Kalenderfunktionen |
| `storage.cpp` | Speicherung der Einträge (LittleFS) und laufender Status (NVS) |
| `net.cpp` | Heim-WLAN, NTP, Hotspot, DNS für Captive Portal |
| `web.cpp`, `page.h` | Webserver und Übersichtsseite |
| `exporter.cpp` | Excel- (.xlsx) und CSV-Erzeugung direkt auf dem Gerät |

Pinbelegung (laut Waveshare-V2-Beispielcode): Display DC 10, CS 11, SCK 12,
MOSI 13, RST 9, BUSY 8, Display-Power 6 · BOOT-Taste 0 · PWR-Taste 18 ·
Akku-Selbsthaltung 17 · LED 3 (low-aktiv) · Akku-ADC 4 (Teiler 1:2) ·
I²C SDA 47 / SCL 48 (RTC 0x51).
