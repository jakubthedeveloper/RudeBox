#include "synth_engine.h"

#include <Arduino.h>

#include "app_config.h"
#include "audio_io.h"
#include "hit_detector.h"
#include "output_limiter.h"
#include "synth_controls.h"
#include "synth_voice.h"

namespace SynthEngine {
namespace {

void reportInputPeak(uint16_t peak) {
  if (AppConfig::Diagnostics::LOG_AUDIO_PEAKS) {
    Serial.printf(">peak:%u\n", peak);
  }
}

bool triggerVoiceForDetectedPadHit(uint16_t peak) {
  if (!HitDetector::update(peak)) return false;

  const SynthControls controls = SynthControlInput::snapshot();
  const float velocity =
      SynthControlInput::velocityFromPeak(peak, controls.sensitivity);
  SynthVoice::trigger(velocity, controls.oscPitchHz,
                      controls.pitchDropOctaves);
  return true;
}

bool processAudioInput() {
  uint16_t peak;
  if (!AudioIo::readPeak(AppConfig::AudioInput::CHANNEL, peak)) return false;

  reportInputPeak(peak);
  return triggerVoiceForDetectedPadHit(peak);
}

void renderAudioOutput() {
  const int16_t* synthesizedSamples = SynthVoice::render();
  AudioIo::write(OutputLimiter::process(synthesizedSamples));
}

}  // namespace

bool processAudioBlock() {
  const bool padHitDetected = processAudioInput();
  renderAudioOutput();
  return padHitDetected;
}

}  // namespace SynthEngine
