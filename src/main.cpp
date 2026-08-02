#include <Arduino.h>

#include "audio_input.h"
#include "es8388.h"

namespace {

constexpr AudioInput::Channel INPUT_CHANNEL = AudioInput::Channel::Right;

void stop(const char* message) {
  Serial.println(message);
  while (true) delay(1000);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  // I2S starts first because the ES8388 needs MCLK during initialization.
  if (!AudioInput::begin()) stop("[FATAL] I2S initialization failed");
  if (!Es8388::begin()) stop("[FATAL] ES8388 initialization failed");
}

void loop() {
  uint16_t peak;
  if (AudioInput::readPeak(INPUT_CHANNEL, peak)) {
    Serial.printf(">peak:%u\n", peak);
  }
}
