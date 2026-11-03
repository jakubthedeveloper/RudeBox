#include "synth_engine.h"

#include <Arduino.h>

#include "app_config.h"
#include "audio_io.h"
#include "hit_detector.h"
#include "output_limiter.h"
#include "pad_waveform_capture.h"
#include "synth_voice.h"

namespace SynthEngine {
namespace {

AudioIo::SampleBlock inputBlock;

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
                ">candidateSign:%d\n"
                ">polarityRejected:%u\n"
                ">triggerAccepted:%u\n"
                ">validationMax:%u\n"
                ">velocity:%.4f\n"
                ">activeSamples:%u\n"
                ">tailMaximum:%u\n"
                ">tailActiveSamples:%u\n"
                ">windowEnergy:%lu\n",
                result.candidateStarted, result.candidateSign,
                result.polarityRejected, result.hitDetected,
                result.validationMax, result.velocity,
                static_cast<unsigned>(result.activeSamples),
                static_cast<unsigned>(result.tailMaximum),
                static_cast<unsigned>(result.tailActiveSamples),
                static_cast<unsigned long>(result.windowEnergy));
}

void reportPadWaveform(const HitDetector::Result& result) {
  if (AppConfig::Diagnostics::LOG_PAD_WAVEFORM) {
    PadWaveformCapture::process(inputBlock.samples, inputBlock.sampleCount,
                                result.candidateStarted);
  }
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
  if (!AudioIo::readSampleBlock(AppConfig::AudioInput::CHANNEL, inputBlock)) {
    return {};
  }

  const HitDetector::Result result = HitDetector::process(
      inputBlock.samples, inputBlock.sampleCount, controls.sensitivity,
      AppConfig::HitDetection::TRIGGER_POLARITY);
  reportInputPeak(result);
  reportTriggerValidation(result);
  reportPadWaveform(result);
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
