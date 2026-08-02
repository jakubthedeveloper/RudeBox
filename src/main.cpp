#include <Arduino.h>

#include "app_config.h"
#include "audio_io.h"
#include "es8388.h"
#include "hit_detector.h"
#include "synth_voice.h"

namespace {

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
}

void loop() {
  uint16_t peak;
  if (AudioIo::readPeak(AppConfig::INPUT_CHANNEL, peak)) {
    float velocity;
    if (HitDetector::update(peak, velocity)) {
      SynthVoice::trigger(velocity);
    }
    Serial.printf(">peak:%u\n", peak);
  }

  AudioIo::write(SynthVoice::render());
}
