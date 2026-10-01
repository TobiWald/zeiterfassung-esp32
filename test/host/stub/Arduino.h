// Minimaler Arduino-Ersatz für Host-Tests (Linux)
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <algorithm>
#include <ctime>
#include <cmath>
#define radians(d) ((d)*M_PI/180.0)
#define PROGMEM
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define pgm_read_word(a) (*(const uint16_t *)(a))
#define pgm_read_dword(a) (*(const uint32_t *)(a))
#define pgm_read_pointer(a) (*(void *const *)(a))
using std::min; using std::max;
typedef bool boolean;
uint32_t millis();
inline void delay(uint32_t) {}
inline void yield() {}
class __FlashStringHelper;

class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &c) : s(c) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(double v, int d = 2) { char b[32]; snprintf(b, 32, "%.*f", d, v); s = b; }
  const char *c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  void reserve(size_t n) { s.reserve(n); }
  int indexOf(char c, int from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  String substring(int a) const { return String(s.substr(a)); }
  String substring(int a, int b) const { return String(s.substr(a, b - a)); }
  long toInt() const { return atol(s.c_str()); }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *o) { s += o; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  String &operator+=(int v) { s += std::to_string(v); return *this; }
  String &operator+=(unsigned v) { s += std::to_string(v); return *this; }
  String &operator+=(long v) { s += std::to_string(v); return *this; }
  String &operator+=(unsigned long v) { s += std::to_string(v); return *this; }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator!=(const String &o) const { return s != o.s; }
  char operator[](size_t i) const { return s[i]; }
  std::string::const_iterator begin() const { return s.begin(); }
  std::string::const_iterator end() const { return s.end(); }
};
inline String operator+(const String &a, const String &b) { return String(a.s + b.s); }
inline String operator+(const String &a, const char *b) { return String(a.s + b); }
inline String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
  size_t print(const char *s) { return write((const uint8_t *)s, strlen(s)); }
};
