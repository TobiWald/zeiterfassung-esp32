#include "timeutil.h"
#include <Wire.h>
#include <sys/time.h>
#include "config.h"

namespace {

const time_t kMinValid = 1735689600;  // 01.01.2025

uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// Tage seit 1970-01-01 für ein UTC-Datum
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

bool rtcOk = false;

bool rtcRead(time_t *out) {
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x04);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(RTC_I2C_ADDR, 7) != 7) return false;
  uint8_t r[7];
  for (int i = 0; i < 7; i++) r[i] = Wire.read();
  if (r[0] & 0x80) return false;  // Oszillator war gestoppt -> Zeit ungültig
  int sec = bcd2dec(r[0] & 0x7F);
  int min = bcd2dec(r[1] & 0x7F);
  int hour = bcd2dec(r[2] & 0x3F);
  int day = bcd2dec(r[3] & 0x3F);
  int mon = bcd2dec(r[5] & 0x1F);
  int year = 2000 + bcd2dec(r[6]);
  if (mon < 1 || mon > 12 || day < 1 || day > 31) return false;
  *out = (time_t)(daysFromCivil(year, mon, day) * 86400 + hour * 3600 + min * 60 + sec);
  return true;
}

void rtcWrite(time_t utc) {
  struct tm t;
  gmtime_r(&utc, &t);
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x00);
  Wire.write(0x00);  // Control_1: normaler Betrieb, 24h
  Wire.endTransmission();
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x04);
  Wire.write(dec2bcd(t.tm_sec));  // löscht auch das OS-Flag
  Wire.write(dec2bcd(t.tm_min));
  Wire.write(dec2bcd(t.tm_hour));
  Wire.write(dec2bcd(t.tm_mday));
  Wire.write(dec2bcd(t.tm_wday));
  Wire.write(dec2bcd(t.tm_mon + 1));
  Wire.write(dec2bcd(t.tm_year % 100));
  rtcOk = Wire.endTransmission() == 0;
}

}  // namespace

namespace timeutil {

void begin() {
  setenv("TZ", TZ_INFO, 1);
  tzset();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  time_t t;
  if (rtcRead(&t) && t > kMinValid) {
    struct timeval tv = {t, 0};
    settimeofday(&tv, nullptr);
    rtcOk = true;
  }
}

bool valid() { return time(nullptr) > kMinValid; }
time_t now() { return time(nullptr); }

void set(time_t utc) {
  struct timeval tv = {utc, 0};
  settimeofday(&tv, nullptr);
  rtcWrite(utc);
}

void saveToRtc() {
  if (valid()) rtcWrite(time(nullptr));
}

struct tm local(time_t t) {
  struct tm r;
  localtime_r(&t, &r);
  return r;
}

time_t dayStart(time_t t) {
  struct tm r = local(t);
  r.tm_hour = 0; r.tm_min = 0; r.tm_sec = 0; r.tm_isdst = -1;
  return mktime(&r);
}

time_t addDays(time_t ds, int days) {
  struct tm r = local(ds);
  r.tm_mday += days;
  r.tm_hour = 0; r.tm_min = 0; r.tm_sec = 0; r.tm_isdst = -1;
  return mktime(&r);
}

time_t weekStart(time_t t) {
  time_t d = dayStart(t);
  int wd = local(d).tm_wday;  // 0 = So
  return addDays(d, -((wd + 6) % 7));
}

time_t monthStart(time_t t) {
  struct tm r = local(t);
  r.tm_mday = 1; r.tm_hour = 0; r.tm_min = 0; r.tm_sec = 0; r.tm_isdst = -1;
  return mktime(&r);
}

time_t monthStart(int year, int month) {
  struct tm r = {};
  r.tm_year = year - 1900; r.tm_mon = month - 1; r.tm_mday = 1; r.tm_isdst = -1;
  return mktime(&r);
}

time_t addMonths(time_t ms, int months) {
  struct tm r = local(ms);
  r.tm_mon += months;
  r.tm_mday = 1; r.tm_hour = 0; r.tm_min = 0; r.tm_sec = 0; r.tm_isdst = -1;
  return mktime(&r);
}

int isoWeek(time_t t, int *isoYear) {
  struct tm r = local(t);
  int wday = (r.tm_wday + 6) % 7;  // 0 = Mo
  // Donnerstag derselben Woche bestimmt Jahr und KW
  struct tm th = r;
  th.tm_mday += 3 - wday;
  th.tm_hour = 12; th.tm_isdst = -1;
  time_t tt = mktime(&th);
  struct tm thu = local(tt);
  if (isoYear) *isoYear = thu.tm_year + 1900;
  return thu.tm_yday / 7 + 1;
}

String fmtDuration(uint32_t sec) {
  char b[16];
  snprintf(b, sizeof(b), "%lu:%02lu", (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60));
  return String(b);
}

String fmtClock(time_t t) {
  struct tm r = local(t);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", r.tm_hour, r.tm_min);
  return String(b);
}

String fmtDate(time_t t) {
  struct tm r = local(t);
  char b[40];
  snprintf(b, sizeof(b), "%02d.%02d.%04d", r.tm_mday, r.tm_mon + 1, r.tm_year + 1900);
  return String(b);
}

const char *weekdayShort(int wday) {
  static const char *n[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  return n[wday % 7];
}

const char *monthName(int month) {
  static const char *n[] = {"Januar", "Februar", "M\xC3\xA4rz", "April", "Mai", "Juni", "Juli",
                            "August", "September", "Oktober", "November", "Dezember"};
  return n[(month - 1) % 12];
}

}  // namespace timeutil
