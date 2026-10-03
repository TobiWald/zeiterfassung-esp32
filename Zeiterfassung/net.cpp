#include "net.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_sntp.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
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
uint32_t staBeginAt = 0;

// Vom Benutzer gespeicherte WLANs (zusätzlich zum fest eingebauten aus secrets.h)
std::vector<net::Network> saved;
Preferences wifiPrefs;
const size_t kMaxSaved = 5;

void loadSaved() {
  saved.clear();
  int n = wifiPrefs.getUChar("n", 0);
  for (int i = 0; i < n; i++) {
    String k = String(i);
    net::Network w{wifiPrefs.getString(("s" + k).c_str(), ""), wifiPrefs.getString(("p" + k).c_str(), "")};
    if (w.ssid.length()) saved.push_back(w);
  }
}

void storeSaved() {
  for (size_t i = 0; i < kMaxSaved; i++) {
    String k = String((int)i);
    if (i < saved.size()) {
      wifiPrefs.putString(("s" + k).c_str(), saved[i].ssid);
      wifiPrefs.putString(("p" + k).c_str(), saved[i].pass);
    } else {
      wifiPrefs.remove(("s" + k).c_str());
      wifiPrefs.remove(("p" + k).c_str());
    }
  }
  wifiPrefs.putUChar("n", saved.size());
}

// Alle bekannten Netze: gespeicherte zuerst, dann das eingebaute
std::vector<net::Network> knownNetworks() {
  std::vector<net::Network> all = saved;
  bool haveBuiltin = false;
  for (auto &w : all) haveBuiltin |= w.ssid == WIFI_SSID;
  if (!haveBuiltin && strlen(WIFI_SSID)) all.push_back({WIFI_SSID, WIFI_PASS});
  return all;
}

// Mit dem stärksten bekannten Netz in Reichweite verbinden
void connectBest() {
  std::vector<net::Network> known = knownNetworks();
  if (known.empty()) return;
  const net::Network *best = &known[0];
  if (known.size() > 1) {
    int n = WiFi.scanNetworks(false, true);
    int bestRssi = -1000;
    for (int i = 0; i < n; i++) {
      for (auto &k : known) {
        if (WiFi.SSID(i) == k.ssid && WiFi.RSSI(i) > bestRssi) {
          bestRssi = WiFi.RSSI(i);
          best = &k;
        }
      }
    }
    WiFi.scanDelete();
  }
  WiFi.begin(best->ssid.c_str(), best->pass.c_str());
  staBeginAt = millis();
}

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
    staBegun = true;
    connectBest();
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
  wifiPrefs.begin("wifi", false);
  loadSaved();
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

  // nicht verbunden: nach 60 s erneut das beste bekannte Netz suchen
  if (staBegun && !connected && millis() - staBeginAt > 60000 && knownNetworks().size() > 1) {
    WiFi.disconnect(false);
    connectBest();
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

std::vector<Network> savedNetworks() { return saved; }
String currentSsid() { return staConnected() ? WiFi.SSID() : String(""); }

bool addNetwork(const String &ssid, const String &pass) {
  if (!ssid.length() || ssid.length() > 32 || pass.length() > 63) return false;
  for (auto it = saved.begin(); it != saved.end(); ++it) {
    if (it->ssid == ssid) {
      saved.erase(it);
      break;
    }
  }
  saved.insert(saved.begin(), {ssid, pass});  // neuestes zuerst
  if (saved.size() > kMaxSaved) saved.resize(kMaxSaved);
  storeSaved();
  // Nicht verbunden? Dann jetzt mit dem neuen Netz versuchen. Uhrzeit holen.
  if (staBegun && !staConnected()) {
    WiFi.disconnect(false);
    WiFi.begin(ssid.c_str(), pass.c_str());
    staBeginAt = millis();
  }
  requestTimeSync();
  return true;
}

bool removeNetwork(const String &ssid) {
  for (auto it = saved.begin(); it != saved.end(); ++it) {
    if (it->ssid == ssid) {
      saved.erase(it);
      storeSaved();
      return true;
    }
  }
  return false;
}

int scanStart() {
  int r = WiFi.scanComplete();
  if (r == WIFI_SCAN_RUNNING) return r;
  if (r >= 0) return r;  // Ergebnis liegt vor
  WiFi.scanNetworks(true, false);  // asynchron, schaltet STA bei Bedarf zu
  return WIFI_SCAN_RUNNING;
}

std::vector<ScanResult> scanResults() {
  std::vector<ScanResult> out;
  int n = WiFi.scanComplete();
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    bool dup = false;
    for (auto &o : out) {
      if (o.ssid == ssid) {
        if (WiFi.RSSI(i) > o.rssi) o.rssi = WiFi.RSSI(i);
        dup = true;
      }
    }
    if (!dup) out.push_back({ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  std::sort(out.begin(), out.end(), [](const ScanResult &a, const ScanResult &b) { return a.rssi > b.rssi; });
  WiFi.scanDelete();
  applyMode();  // falls die Suche das STA-Interface eingeschaltet hat
  return out;
}

String takeNotice() {
  String n = notice;
  notice = "";
  return n;
}

}  // namespace net
