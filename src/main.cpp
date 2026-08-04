#include <Arduino.h>

#include "ads7830.h"
#include "app_config.h"
#include "audio_io.h"
#include "es8388.h"
#include "hit_detector.h"
#include "synth_voice.h"

namespace {

uint32_t lastPotentiometerReportMs = 0;

void stop(const char* message) {
  Serial.println(message);
  while (true) delay(1000);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  // I2S starts first because the ES8388 needs MCLK during initialization.
  if (!AudioIo::begin()) stop("[FATAL] I2S initialization failed");
  if (!Es8388::begin()) stop("[FATAL] ES8388 initialization failed");
  if (!Ads7830::begin()) stop("[FATAL] ADS7830 initialization failed");

  lastPotentiometerReportMs = millis();
}

void loop() {
  uint16_t peak;
  if (AudioIo::readPeak(AppConfig::INPUT_CHANNEL, peak)) {
    float velocity;
    if (HitDetector::update(peak, velocity)) {
      SynthVoice::trigger(velocity);
    }
    if (AppConfig::LOG_AUDIO_PEAKS) Serial.printf(">peak:%u\n", peak);
  }

  AudioIo::write(SynthVoice::render());

  const uint32_t now = millis();
  if (AppConfig::LOG_POTENTIOMETERS &&
      now - lastPotentiometerReportMs >=
      AppConfig::POTENTIOMETER_REPORT_INTERVAL_MS) {
    lastPotentiometerReportMs = now;

    for (uint8_t channel = 0;
         channel < AppConfig::POTENTIOMETER_COUNT &&
         channel < Ads7830::CHANNEL_COUNT;
         ++channel) {
      float value;
      if (Ads7830::readAverage(
              channel, AppConfig::POTENTIOMETER_SAMPLES_PER_READING, value)) {
        Serial.printf("Potentiometer %u: %.2f\n", channel, value);
      } else {
        Serial.printf("[WARN] Failed to read potentiometer %u\n", channel);
      }
    }
  }
}
