#include "synth_engine.h"

#include <Arduino.h>

#include "app_config.h"
#include "audio_io.h"
#include "hit_detector.h"
#include "output_limiter.h"
#include "synth_voice.h"

namespace SynthEngine {
namespace {

AudioIo::MagnitudeBlock inputBlock;

void reportInputPeak(const HitDetector::Result& result) {
  if (AppConfig::Diagnostics::LOG_AUDIO_PEAKS) {
    Serial.printf(">rawPeak:%u\n", result.rawPeak);
  }
}

void reportTriggerValidation(const HitDetector::Result& result) {
  if (!AppConfig::Diagnostics::LOG_TRIGGER_VALIDATION ||
      (!result.candidateStarted && !result.validationCompleted)) {
    return;
  }

  Serial.printf(">triggerCandidate:%u\n"
                ">triggerAccepted:%u\n"
                ">validationMax:%u\n"
                ">activeSamples:%u\n"
                ">windowEnergy:%lu\n",
                result.candidateStarted, result.hitDetected,
                result.validationMax, result.activeSamples,
                static_cast<unsigned long>(result.windowEnergy));
}

bool triggerVoiceForDetectedPadHit(const HitDetector::Result& result,
                                   const SynthControls& controls) {
  if (!result.hitDetected) return false;

  SynthVoice::trigger(result.velocity, controls.oscPitchHz,
                      controls.pitchDropOctaves);
  return true;
}

bool processAudioInput(const SynthControls& controls) {
  if (!AudioIo::readMagnitudeBlock(AppConfig::AudioInput::CHANNEL, inputBlock)) {
    return false;
  }

  const HitDetector::Result result = HitDetector::process(
      inputBlock.samples, inputBlock.sampleCount, controls.sensitivity);
  reportInputPeak(result);
  reportTriggerValidation(result);
  return triggerVoiceForDetectedPadHit(result, controls);
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
