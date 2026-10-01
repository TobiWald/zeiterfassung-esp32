#include "net.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_sntp.h>
#include "config.h"
#include "timeutil.h"
#include "web.h"

namespace {

bool staUser = false;
bool tempSync = false;
uint32_t tempSyncSince = 0;
bool ap = false;
bool staBegun = false;
bool wasConnected = false;
bool webStarted = false;
bool apStarted = false;
volatile bool sntpSynced = false;
uint32_t lastSync = 0;
String notice;
DNSServer dns;

void onSntpSync(struct timeval *) { sntpSynced = true; }

void applyMode() {
  bool sta = staUser || tempSync;
  wifi_mode_t mode = sta && ap ? WIFI_AP_STA : sta ? WIFI_STA : ap ? WIFI_AP : WIFI_OFF;

  if (mode != WIFI_OFF) setCpuFrequencyMhz(160);
  if (!ap && apStarted) {
    dns.stop();
    apStarted = false;
  }
  if (!sta && staBegun) {
    esp_sntp_stop();
    WiFi.disconnect(true);
    staBegun = false;
    wasConnected = false;
  }
  if (WiFi.getMode() != mode) WiFi.mode(mode);
  if (mode == WIFI_OFF) {
    setCpuFrequencyMhz(80);
    return;
  }
  WiFi.setSleep(true);

  if (ap && !apStarted) {
    apStarted = true;
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASS);
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", IPAddress(192, 168, 4, 1));
  }
  if (sta && !staBegun) {
    WiFi.setHostname("zeiterfassung");
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    staBegun = true;
  }
  if (!webStarted) {
    web::begin();
    webStarted = true;
  }
}

}  // namespace

namespace net {

void begin() {
  WiFi.persistent(false);
  sntp_set_time_sync_notification_cb(onSntpSync);
  WiFi.mode(WIFI_OFF);
  setCpuFrequencyMhz(80);
}

void loop() {
  bool connected = WiFi.status() == WL_CONNECTED && (staUser || tempSync);
  if (connected && !wasConnected) {
    wasConnected = true;
    sntpSynced = false;
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    if (staUser) notice = "WLAN verbunden\n" + WiFi.localIP().toString();
  } else if (!connected && wasConnected) {
    wasConnected = false;
  }

  if (sntpSynced) {
    sntpSynced = false;
    timeutil::saveToRtc();
    lastSync = timeutil::now();
    if (tempSync) {
      tempSync = false;
      notice = "Uhrzeit gestellt\n" + timeutil::fmtClock(timeutil::now());
      applyMode();
    }
  }

  if (tempSync && millis() - tempSyncSince > 45000) {
    tempSync = false;
    notice = "Kein WLAN!\nUhrzeit nicht\ngestellt";
    applyMode();
  }

  if (ap) dns.processNextRequest();
  if (webStarted && WiFi.getMode() != WIFI_OFF) web::loop();
}

void setSta(bool on) {
  staUser = on;
  applyMode();
}
bool staWanted() { return staUser; }
bool staActive() { return staUser || tempSync; }
bool staConnected() { return (staUser || tempSync) && WiFi.status() == WL_CONNECTED; }
String staIp() { return WiFi.localIP().toString(); }

void setAp(bool on) {
  ap = on;
  applyMode();
}
bool apOn() { return ap; }
String apIp() { return WiFi.softAPIP().toString(); }

void requestTimeSync() {
  if (staUser && WiFi.status() == WL_CONNECTED) {
    sntpSynced = false;
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    return;
  }
  tempSync = true;
  tempSyncSince = millis();
  applyMode();
}

bool timeSyncPending() { return tempSync; }

bool syncTimeBlocking(uint32_t timeoutMs) {
  uint32_t before = lastSync;
  requestTimeSync();
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    loop();
    if (lastSync != before) return true;
    if (!tempSync && !staUser) break;
    delay(20);
  }
  return lastSync != before;
}

uint32_t lastSyncEpoch() { return lastSync; }

String takeNotice() {
  String n = notice;
  notice = "";
  return n;
}

}  // namespace net
