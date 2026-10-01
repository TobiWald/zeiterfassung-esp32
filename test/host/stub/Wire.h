#pragma once
#include <Arduino.h>
struct TwoWire {
  void begin(int, int, uint32_t) {}
  void beginTransmission(int) {}
  size_t write(uint8_t) { return 1; }
  uint8_t endTransmission(bool = true) { return 1; }
  uint8_t requestFrom(int, int) { return 0; }
  int read() { return 0; }
};
inline TwoWire Wire;
