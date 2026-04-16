/**
 * Speaker test pour Gotchi — basé sur l'exemple Waveshare 08_ES8311.
 * Utilise ESP_I2S.h (Arduino v3.x) + driver ES8311 Waveshare natif.
 */
#include "gotchi_speaker_test.h"
#include "../config/config.h"
#include <Arduino.h>
#include "sounds/sound_eating.h"
#include <Wire.h>
#include <cmath>
#include "ESP_I2S.h"
#include "esp_check.h"
#include "esp_heap_caps.h"

// Wrapper pour forcer heap_caps_calloc en RAM interne pendant l'init I2S
static volatile bool s_forceInternal = false;

extern "C" void* __real_heap_caps_calloc(size_t n, size_t size, uint32_t caps);
extern "C" void* __wrap_heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
  if (s_forceInternal && caps == MALLOC_CAP_DEFAULT) {
    return __real_heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  return __real_heap_caps_calloc(n, size, caps);
}

// Driver ES8311 Waveshare (utilise i2cWrite/i2cWriteReadNonStop)
extern "C" {
  #include "es8311.h"
}

#include "common/managers/sd/sd_manager.h"

// ============================================
// Config (identique à l'exemple Waveshare)
// ============================================

#define SPEAKER_SAMPLE_RATE   16000
#define SPEAKER_VOICE_VOLUME  90
#define SPEAKER_MIC_GAIN      ((es8311_mic_gain_t)3)

// ============================================
// ES8311 init via le driver Waveshare
// ============================================

static esp_err_t codecInit() {
  es8311_handle_t handle = es8311_create(0, ES8311_ADDRRES_0);
  if (!handle) {
    Serial.println("[SPEAKER] es8311_create failed");
    return ESP_FAIL;
  }

  const es8311_clock_config_t clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = SPEAKER_SAMPLE_RATE * 256,
    .sample_frequency = SPEAKER_SAMPLE_RATE
  };

  esp_err_t err;
  err = es8311_init(handle, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (err != ESP_OK) { Serial.printf("[SPEAKER] es8311_init: %s\n", esp_err_to_name(err)); return err; }

  err = es8311_sample_frequency_config(handle, clk.mclk_frequency, clk.sample_frequency);
  if (err != ESP_OK) { Serial.printf("[SPEAKER] freq_config: %s\n", esp_err_to_name(err)); return err; }

  es8311_microphone_config(handle, false);

  // Volume depuis config.json (0-100)
  SDConfig cfg = SDManager::getConfig();
  int vol = cfg.speaker_volume;
  es8311_voice_volume_set(handle, vol, NULL);
  Serial.printf("[SPEAKER] Volume: %d%%\n", vol);

  es8311_microphone_gain_set(handle, SPEAKER_MIC_GAIN);

  Serial.println("[SPEAKER] ES8311 OK (driver Waveshare)");
  return ESP_OK;
}

// ============================================
// I2S via ESP_I2S.h (Arduino v3.x)
// Pins identiques à l'exemple Waveshare:
//   BCLK=9, WS=45, DIN=8, DOUT=10, MCLK=42
// ============================================

static I2SClass* s_i2s = nullptr;

static bool initI2S() {
  // Forcer TOUTES les allocations en RAM interne pendant l'init I2S
  // (le driver GDMA refuse les callbacks en PSRAM)
  s_forceInternal = true;

  s_i2s = (I2SClass*)heap_caps_malloc(sizeof(I2SClass), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!s_i2s) {
    s_forceInternal = false;
    Serial.println("[SPEAKER] I2S malloc failed");
    return false;
  }
  new (s_i2s) I2SClass();

  s_i2s->setPins(GOTCHI_I2S_BCK_IO, GOTCHI_I2S_WS_IO, GOTCHI_I2S_DI_IO, GOTCHI_I2S_DO_IO, GOTCHI_I2S_MCK_IO);

  if (!s_i2s->begin(I2S_MODE_STD, SPEAKER_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("[SPEAKER] I2S begin failed");
    s_i2s->~I2SClass();
    heap_caps_free(s_i2s);
    s_i2s = nullptr;
    s_forceInternal = false;
    return false;
  }

  s_forceInternal = false;

  Serial.printf("[SPEAKER] I2S OK (BCK=%d WS=%d DOUT=%d MCLK=%d)\n",
    GOTCHI_I2S_BCK_IO, GOTCHI_I2S_WS_IO, GOTCHI_I2S_DO_IO, GOTCHI_I2S_MCK_IO);
  return true;
}

static void deinitI2S() {
  if (s_i2s) {
    s_i2s->end();
    s_i2s->~I2SClass();
    heap_caps_free(s_i2s);
    s_i2s = nullptr;
  }
}

// ============================================
// PA
// ============================================

static void enablePA() {
  pinMode(GOTCHI_PA_PIN, OUTPUT);
  digitalWrite(GOTCHI_PA_PIN, HIGH);
  delay(10);
}

static void disablePA() {
  digitalWrite(GOTCHI_PA_PIN, LOW);
}

// ============================================
// Tone generation
// ============================================

static void generateTone(uint16_t freqHz, uint16_t durationMs, uint8_t volumePercent) {
  const int totalSamples = (SPEAKER_SAMPLE_RATE * durationMs) / 1000;
  const float amplitude = 32767.0f * (volumePercent / 100.0f);
  const float twoPiF = 2.0f * M_PI * freqHz;

  constexpr int FRAMES = 128;
  int16_t buf[FRAMES * 2];  // stereo
  int written = 0;

  while (written < totalSamples) {
    int chunk = min(FRAMES, totalSamples - written);
    for (int i = 0; i < chunk; i++) {
      float t = (float)(written + i) / SPEAKER_SAMPLE_RATE;
      int16_t s = (int16_t)(amplitude * sinf(twoPiF * t));
      buf[i * 2] = s;
      buf[i * 2 + 1] = s;
    }
    s_i2s->write((uint8_t*)buf, chunk * 4);
    written += chunk;
  }

  // Silence pour éviter le click
  memset(buf, 0, sizeof(buf));
  s_i2s->write((uint8_t*)buf, sizeof(buf));
}

// ============================================
// Public API
// ============================================

namespace GotchiSpeakerTest {

bool scanES8311() {
  Wire.beginTransmission(0x18);
  bool found = Wire.endTransmission() == 0;
  Serial.printf("[SPEAKER] ES8311 @ 0x18: %s\n", found ? "OK" : "NOT FOUND");
  return found;
}

bool dumpRegisters() {
  return scanES8311();
}

bool playTone(uint16_t freqHz, uint16_t durationMs, uint8_t volume) {
  Serial.printf("[SPEAKER] Tone %dHz %dms vol=%d%%\n", freqHz, durationMs, volume);

  // Séquence identique à Waveshare 08_ES8311:
  // 1. PA ON
  enablePA();

  // 2. I2S (MCLK commence à tourner)
  if (!initI2S()) { disablePA(); return false; }

  // 3. Wire + codec init
  Wire.begin(IIC_SDA, IIC_SCL);
  if (codecInit() != ESP_OK) { deinitI2S(); disablePA(); return false; }

  // 4. Jouer
  generateTone(freqHz, durationMs, volume);

  // 5. Cleanup
  delay(50);
  disablePA();
  deinitI2S();
  Serial.println("[SPEAKER] Done");
  return true;
}

bool playSound(const uint8_t* pcmData, uint32_t pcmLen) {
  Serial.printf("[SPEAKER] Sound %lu bytes\n", pcmLen);

  enablePA();
  if (!initI2S()) { disablePA(); return false; }
  Wire.begin(IIC_SDA, IIC_SCL);
  if (codecInit() != ESP_OK) { deinitI2S(); disablePA(); return false; }

  // Le PCM est mono 16-bit. I2S attend du stereo → dupliquer L+R
  constexpr int FRAMES = 64;
  int16_t stereoBuf[FRAMES * 2];
  uint32_t offset = 0;

  while (offset < pcmLen) {
    int monoBytes = min((uint32_t)(FRAMES * 2), pcmLen - offset);
    int monoSamples = monoBytes / 2;
    const int16_t* mono = (const int16_t*)(pcmData + offset);

    for (int i = 0; i < monoSamples; i++) {
      stereoBuf[i * 2] = mono[i];
      stereoBuf[i * 2 + 1] = mono[i];
    }

    s_i2s->write((uint8_t*)stereoBuf, monoSamples * 4);
    offset += monoBytes;
  }

  // Silence
  memset(stereoBuf, 0, sizeof(stereoBuf));
  s_i2s->write((uint8_t*)stereoBuf, sizeof(stereoBuf));

  delay(50);
  disablePA();
  deinitI2S();
  Serial.println("[SPEAKER] Done");
  return true;
}

// --- Async (non-bloquant) ---
struct AsyncSoundParams {
  const uint8_t* data;
  uint32_t len;
};

static void asyncSoundTask(void* param) {
  AsyncSoundParams* p = (AsyncSoundParams*)param;
  playSound(p->data, p->len);
  delete p;
  vTaskDelete(NULL);
}

void playSoundAsync(const uint8_t* pcmData, uint32_t pcmLen) {
  AsyncSoundParams* p = new AsyncSoundParams{pcmData, pcmLen};
  xTaskCreatePinnedToCore(asyncSoundTask, "snd", 16384, p, 1, NULL, 1);
}

bool playMelody() {
  Serial.println("[SPEAKER] Melodie...");

  enablePA();
  if (!initI2S()) { disablePA(); return false; }
  Wire.begin(IIC_SDA, IIC_SCL);
  if (codecInit() != ESP_OK) { deinitI2S(); disablePA(); return false; }

  static const uint16_t notes[] = {262, 294, 330, 349, 392};
  static const char* names[] = {"Do", "Re", "Mi", "Fa", "Sol"};
  for (int i = 0; i < 5; i++) {
    Serial.printf("[SPEAKER] %s (%d)\n", names[i], notes[i]);
    generateTone(notes[i], 400, 90);
    delay(80);
  }

  delay(50);
  disablePA();
  deinitI2S();
  Serial.println("[SPEAKER] Done");
  return true;
}

void playEatingSound() {
  playSoundAsync(EATING_PCM, EATING_PCM_LEN);
}

} // namespace GotchiSpeakerTest
