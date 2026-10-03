#include "hw.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include "config.h"

namespace {

QueueHandle_t queue;
volatile bool ledBlink = false;
volatile int ledFlashCount = 0;

struct Btn {
  uint8_t pin;
  BtnId id;
  bool holdEvents;      // lange Haltezeiten auswerten
  bool stable = false;  // true = gedrückt
  bool raw = false;
  uint32_t rawSince = 0;
  uint32_t pressedAt = 0;
  uint32_t releasedAt = 0;
  uint8_t clicks = 0;
  uint32_t reachedStage = 0;
  bool ignoreUntilRelease = false;
};

Btn btns[2] = {
    {PIN_BTN_TRACK, BTN_TRACK, false},
    {PIN_BTN_MENU, BTN_MENU, true},
};

void post(BtnId id, BtnEvType t, uint32_t v) {
  BtnEvent e{id, t, v};
  xQueueSend(queue, &e, 0);
}

void pollButton(Btn &b, uint32_t now) {
  bool r = digitalRead(b.pin) == LOW;
  if (r != b.raw) {
    b.raw = r;
    b.rawSince = now;
  }
  if (b.raw != b.stable && now - b.rawSince >= 25) {  // Entprellen
    b.stable = b.raw;
    if (b.stable) {
      b.pressedAt = now;
      b.reachedStage = 0;
    } else {
      uint32_t held = now - b.pressedAt;
      if (b.ignoreUntilRelease) {
        b.ignoreUntilRelease = false;
      } else if (held < 800) {
        b.clicks++;
        b.releasedAt = now;
      } else if (b.holdEvents && held >= AP_HOLD_MS) {
        post(b.id, EV_HOLD_RELEASED, held);
      }
    }
  }
  if (b.stable && !b.ignoreUntilRelease) {
    uint32_t held = now - b.pressedAt;
    if (held >= 800) b.clicks = 0;  // langer Druck verwirft Klickserie
    if (b.holdEvents) {
      if (held >= POWEROFF_HOLD_MS && b.reachedStage < POWEROFF_HOLD_MS) {
        b.reachedStage = POWEROFF_HOLD_MS;
        post(b.id, EV_HOLD_REACHED, POWEROFF_HOLD_MS);
      } else if (held >= AP_HOLD_MS && b.reachedStage < AP_HOLD_MS) {
        b.reachedStage = AP_HOLD_MS;
        post(b.id, EV_HOLD_REACHED, AP_HOLD_MS);
      }
    }
  }
  if (!b.stable && b.clicks && now - b.releasedAt > CLICK_GAP_MS) {
    post(b.id, EV_CLICKS, b.clicks);
    b.clicks = 0;
  }
}

void ledWrite(bool on) { digitalWrite(PIN_LED, on ? LOW : HIGH); }

void task(void *) {
  uint32_t flashUntil = 0;
  for (;;) {
    uint32_t now = millis();
    for (auto &b : btns) pollButton(b, now);

    if (ledFlashCount > 0 && flashUntil == 0) {
      flashUntil = now + ledFlashCount * 200;
      ledFlashCount = 0;
    }
    if (flashUntil) {
      if ((int32_t)(now - flashUntil) >= 0) {
        flashUntil = 0;
      } else {
        ledWrite(((flashUntil - now) / 100) % 2);
      }
    } else if (ledBlink) {
      ledWrite(now % LED_PERIOD_MS < LED_ON_MS);
    } else {
      ledWrite(false);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace

namespace hw {

void earlyInit() {
  pinMode(PIN_VBAT_HOLD, OUTPUT);
  digitalWrite(PIN_VBAT_HOLD, HIGH);  // Akku-Versorgung halten
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  digitalWrite(PIN_AUDIO_PWR, HIGH);  // Audio-Codec aus
  pinMode(PIN_PA_EN, OUTPUT);
  digitalWrite(PIN_PA_EN, LOW);       // Verstärker aus
  pinMode(PIN_LED, OUTPUT);
  ledWrite(false);
  pinMode(PIN_BTN_TRACK, INPUT_PULLUP);
  pinMode(PIN_BTN_MENU, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
}

void begin() {
  queue = xQueueCreate(16, sizeof(BtnEvent));
  // Eine beim Start gedrückte Taste (z.B. Einschalten per PWR) ignorieren
  for (auto &b : btns) {
    if (digitalRead(b.pin) == LOW) {
      b.stable = b.raw = true;
      b.ignoreUntilRelease = true;
    }
  }
  xTaskCreatePinnedToCore(task, "hw", 3072, nullptr, 5, nullptr, 0);
}

bool nextEvent(BtnEvent *ev) { return xQueueReceive(queue, ev, 0) == pdTRUE; }
void setLedBlink(bool on) { ledBlink = on; }
void ledFlash(int times) { ledFlashCount = times * 2; }
bool menuButtonDown() { return digitalRead(PIN_BTN_MENU) == LOW; }

float batteryVoltage() {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(PIN_BAT_ADC);
  return mv / 16.0f * 2.0f / 1000.0f;
}

int batteryPercent(float v) {
  static const float tbl[][2] = {{3.30f, 0},  {3.60f, 10}, {3.70f, 25}, {3.78f, 45},
                                 {3.88f, 62}, {3.98f, 78}, {4.08f, 92}, {4.15f, 100}};
  if (v <= tbl[0][0]) return 0;
  for (int i = 1; i < 8; i++) {
    if (v < tbl[i][0]) {
      float f = (v - tbl[i - 1][0]) / (tbl[i][0] - tbl[i - 1][0]);
      return (int)(tbl[i - 1][1] + f * (tbl[i][1] - tbl[i - 1][1]));
    }
  }
  return 100;
}

void powerOff() {
  ledWrite(false);
  digitalWrite(PIN_VBAT_HOLD, LOW);
}

void powerHold() { digitalWrite(PIN_VBAT_HOLD, HIGH); }

void deepSleep() {
  ledWrite(false);
  uint32_t t0 = millis();
  while (digitalRead(PIN_BTN_MENU) == LOW && millis() - t0 < 10000) delay(10);  // Taste loslassen
  delay(50);
  rtc_gpio_pullup_en((gpio_num_t)PIN_BTN_MENU);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BTN_MENU);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_sleep_enable_ext1_wakeup(1ULL << PIN_BTN_MENU, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

}  // namespace hw
