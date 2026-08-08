#include "application.h"

#include <Arduino.h>

#include "audio_io.h"
#include "es8388.h"
#include "synth_engine.h"
#include "user_interface.h"

namespace Application {
namespace {

void stop(const char* message) {
  Serial.println(message);
  while (true) delay(1000);
}

void initializeSerial() {
  Serial.begin(115200);
  delay(500);
}

void initializeAudio() {
  // I2S starts first because the ES8388 needs MCLK during initialization.
  if (!AudioIo::begin()) stop("[FATAL] I2S initialization failed");
  if (!Es8388::begin()) stop("[FATAL] ES8388 initialization failed");
}

void initializeUserInterface() {
  if (!UserInterface::begin()) {
    stop("[FATAL] User interface initialization failed");
  }
}

}  // namespace

void begin() {
  initializeSerial();
  initializeAudio();
  initializeUserInterface();
}

void update() {
  const SynthControls controls = UserInterface::synthControls();
  const SynthEngine::ProcessResult result =
      SynthEngine::processAudioBlock(controls);
  if (result.hitDetected) UserInterface::indicatePadHit(result.velocity);
  UserInterface::update();
}

}  // namespace Application
