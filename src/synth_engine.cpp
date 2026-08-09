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
                ">velocity:%.4f\n"
                ">activeSamples:%u\n"
                ">windowEnergy:%lu\n",
                result.candidateStarted, result.hitDetected,
                result.validationMax, result.velocity, result.activeSamples,
                static_cast<unsigned long>(result.windowEnergy));
}

ProcessResult triggerVoiceForDetectedPadHit(const HitDetector::Result& result,
                                            const SynthControls& controls) {
  if (!result.hitDetected) return {};

  SynthVoice::trigger(result.velocity, controls.oscPitchHz,
                      controls.pitchDropOctaves, controls.clickLevel,
                      controls.ampVelocity, controls.shapeNormalized,
                      controls.decayMs, controls.envToPitchSemitones);
  return {true, result.velocity};
}

ProcessResult processAudioInput(const SynthControls& controls) {
  if (!AudioIo::readMagnitudeBlock(AppConfig::AudioInput::CHANNEL, inputBlock)) {
    return {};
  }

  const HitDetector::Result result = HitDetector::process(
      inputBlock.samples, inputBlock.sampleCount, controls.sensitivity);
  reportInputPeak(result);
  reportTriggerValidation(result);
  return triggerVoiceForDetectedPadHit(result, controls);
}

void renderAudioOutput() {
  const int32_t* synthesizedSamples = SynthVoice::render();
  AudioIo::writeMono(OutputLimiter::process(synthesizedSamples));
}

}  // namespace

ProcessResult processAudioBlock(const SynthControls& controls) {
  SynthVoice::setShape(controls.shapeNormalized);
  const ProcessResult result = processAudioInput(controls);
  renderAudioOutput();
  return result;
}

}  // namespace SynthEngine
