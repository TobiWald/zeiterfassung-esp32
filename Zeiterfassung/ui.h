#pragma once
#include <Arduino.h>

enum View : uint8_t { V_MAIN, V_DAY, V_WEEK, V_MONTH, V_AP };

namespace ui {
void begin();
void update();                    // im loop() aufrufen
void nextView();
void setView(View v);
View view();
void goHome();
// Großformatige Meldung, Zeilen mit '\n' getrennt. ms = 0 -> bis clearMessage()
void showMessage(const String &text, uint32_t ms = 3000);
void clearMessage();
void requestRedraw();
void drawNow(bool full = false);  // sofort zeichnen (blockiert)
}
