#include "synth_engine.h"

#include <Arduino.h>

#include "app_config.h"
#include "audio_io.h"
#include "hit_detector.h"
#include "output_limiter.h"
#include "synth_voice.h"

namespace SynthEngine {
namespace {

void reportInputPeak(uint16_t peak) {
  if (AppConfig::Diagnostics::LOG_AUDIO_PEAKS) {
    Serial.printf(">peak:%u\n", peak);
  }
}

bool triggerVoiceForDetectedPadHit(uint16_t peak,
                                   const SynthControls& controls) {
  float velocity;
  if (!HitDetector::detect(peak, controls.sensitivity, velocity)) return false;

  SynthVoice::trigger(velocity, controls.oscPitchHz,
                      controls.pitchDropOctaves);
  return true;
}

bool processAudioInput(const SynthControls& controls) {
  uint16_t peak;
  if (!AudioIo::readPeak(AppConfig::AudioInput::CHANNEL, peak)) return false;

  reportInputPeak(peak);
  return triggerVoiceForDetectedPadHit(peak, controls);
}

void renderAudioOutput() {
  const int16_t* synthesizedSamples = SynthVoice::render();
  AudioIo::writeMono(OutputLimiter::process(synthesizedSamples));
}

}  // namespace

bool processAudioBlock(const SynthControls& controls) {
  const bool padHitDetected = processAudioInput(controls);
  renderAudioOutput();
  return padHitDetected;
}

}  // namespace SynthEngine
