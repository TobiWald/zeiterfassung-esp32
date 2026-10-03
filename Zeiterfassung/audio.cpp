// Piepton über ES8311-Codec (I2C 0x18) und I2S. Registerfolge nach dem
// esp_codec_dev-Treiber aus dem Waveshare-Beispiel (16 kHz, MCLK = 256 * fs).
#include "audio.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include "config.h"

namespace {

volatile bool busy = false;

bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

int rd(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

void upd(uint8_t reg, uint8_t keep, uint8_t set) {
  int v = rd(reg);
  wr(reg, ((v < 0 ? 0 : v) & keep) | set);
}

bool codecStart() {
  wr(0x44, 0x08);  // I2C-Störfestigkeit (zweimal, laut Treiber)
  if (!wr(0x44, 0x08)) return false;
  wr(0x01, 0x30); wr(0x02, 0x00); wr(0x03, 0x10); wr(0x16, 0x24);
  wr(0x04, 0x10); wr(0x05, 0x00); wr(0x0B, 0x00); wr(0x0C, 0x00);
  wr(0x10, 0x1F); wr(0x11, 0x7F); wr(0x00, 0x80);  // Slave-Modus
  wr(0x01, 0x3F);                                   // Takt von MCLK-Pin
  upd(0x06, 0xDF, 0x00);
  wr(0x13, 0x10); wr(0x1B, 0x0A); wr(0x1C, 0x6A); wr(0x44, 0x08);
  // I2S, 16 Bit
  upd(0x09, 0xFC, 0x0C);
  upd(0x0A, 0xFC, 0x0C);
  // 16 kHz bei 4,096 MHz MCLK
  upd(0x02, 0x07, 0x00); wr(0x05, 0x00);
  upd(0x03, 0x80, 0x10); upd(0x04, 0x80, 0x20);
  upd(0x07, 0xC0, 0x00); wr(0x08, 0xFF);
  upd(0x06, 0xE0, 0x03);
  // DAC starten
  wr(0x00, 0x80); wr(0x01, 0x3F);
  upd(0x09, 0xBF, 0x00);  // DAC-Eingang an
  upd(0x0A, 0xBF, 0x40);  // ADC-Ausgang aus
  wr(0x17, 0xBF); wr(0x0E, 0x02); wr(0x12, 0x00); wr(0x14, 0x1A);
  wr(0x0D, 0x01); wr(0x15, 0x40); wr(0x37, 0x08); wr(0x45, 0x00);
  wr(0x32, 0xBF);         // Lautstärke 0 dB
  upd(0x31, 0x9F, 0x00);  // Stummschaltung aus
  return true;
}

void codecStop() {
  wr(0x32, 0x00); wr(0x17, 0x00); wr(0x0E, 0xFF); wr(0x12, 0x02);
  wr(0x14, 0x00); wr(0x0D, 0xFA); wr(0x15, 0x00); wr(0x02, 0x10);
  wr(0x00, 0x00); wr(0x00, 0x1F); wr(0x01, 0x30); wr(0x01, 0x00);
  wr(0x45, 0x00); wr(0x0D, 0xFC); wr(0x02, 0x00);
}

void tone(I2SClass &i2s, int ms, bool on) {
  const int rate = 16000, freq = 2000;
  int16_t buf[256 * 2];
  int total = rate * ms / 1000, done = 0;
  static uint32_t phase = 0;
  while (done < total) {
    int n = min(256, total - done);
    for (int i = 0; i < n; i++) {
      int16_t v = 0;
      if (on) {
        int pos = done + i;
        int env = min(min(pos, total - pos), 80);  // weiches Ein-/Ausblenden
        float s = sinf(2.0f * PI * freq * (phase++) / rate);
        v = (int16_t)(s * 14000.0f * env / 80);
      }
      buf[2 * i] = buf[2 * i + 1] = v;
    }
    i2s.write((uint8_t *)buf, n * 4);
    done += n;
  }
}

void task(void *arg) {
  int count = (int)(intptr_t)arg;
  digitalWrite(PIN_AUDIO_PWR, LOW);  // Audio-Versorgung an
  delay(20);
  I2SClass i2s;
  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, -1, PIN_I2S_MCLK);
  if (i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    delay(10);
    if (codecStart()) {
      pinMode(PIN_PA_EN, OUTPUT);
      digitalWrite(PIN_PA_EN, HIGH);
      tone(i2s, 40, false);
      for (int i = 0; i < count; i++) {
        tone(i2s, 120, true);
        tone(i2s, 110, false);
      }
      tone(i2s, 60, false);
      digitalWrite(PIN_PA_EN, LOW);
      codecStop();
    }
    i2s.end();
  }
  digitalWrite(PIN_AUDIO_PWR, HIGH);  // Audio-Versorgung aus
  busy = false;
  vTaskDelete(nullptr);
}

}  // namespace

namespace audio {

void beep(int count) {
  if (busy) return;
  busy = true;
  if (xTaskCreatePinnedToCore(task, "beep", 6144, (void *)(intptr_t)count, 3, nullptr, 0) != pdPASS) busy = false;
}

}  // namespace audio
