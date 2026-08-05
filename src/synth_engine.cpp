#include "synth_engine.h"

#include <Arduino.h>

#include "app_config.h"
#include "audio_io.h"
#include "hit_detector.h"
#include "synth_voice.h"

namespace SynthEngine {
namespace {

void reportInputPeak(uint16_t peak) {
  if (AppConfig::Diagnostics::LOG_AUDIO_PEAKS) {
    Serial.printf(">peak:%u\n", peak);
  }
}

bool triggerVoiceForDetectedPadHit(uint16_t peak) {
  float velocity;
  if (!HitDetector::update(peak, velocity)) return false;

  SynthVoice::trigger(velocity);
  return true;
}

bool processAudioInput() {
  uint16_t peak;
  if (!AudioIo::readPeak(AppConfig::AudioInput::CHANNEL, peak)) return false;

  reportInputPeak(peak);
  return triggerVoiceForDetectedPadHit(peak);
}

void renderAudioOutput() {
  AudioIo::write(SynthVoice::render());
}

}  // namespace

bool processAudioBlock() {
  const bool padHitDetected = processAudioInput();
  renderAudioOutput();
  return padHitDetected;
}

}  // namespace SynthEngine
