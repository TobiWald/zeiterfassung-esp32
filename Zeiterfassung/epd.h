#pragma once
#include <Arduino.h>

// Treiber für das 1,54" ePaper (200x200, SSD1681) des Waveshare
// ESP32-S3-ePaper-1.54. Init-Sequenzen und LUTs stammen aus dem
// offiziellen Waveshare-V2-Beispielcode.
//
// Pufferformat: 1 Bit pro Pixel, zeilenweise, MSB = linkes Pixel,
// 1 = weiß, 0 = schwarz (identisch zu Adafruit GFXcanvas1).

#define EPD_W 200
#define EPD_H 200
#define EPD_BUF_SIZE (EPD_W * EPD_H / 8)

namespace epd {
void begin();
// Vollständiger Refresh (flackert, entfernt Geisterbilder)
void fullRefresh(const uint8_t *buf);
// Schneller Teil-Refresh
void partialRefresh(const uint8_t *buf);
void sleep();
}
