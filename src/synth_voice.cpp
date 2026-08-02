#include "synth_voice.h"

#include <math.h>

#include "app_config.h"
#include "audio_io.h"

namespace SynthVoice {
namespace {

constexpr float SAMPLE_RATE = 44100.0f;
constexpr uint32_t AMP_RELEASE_SAMPLES =
    static_cast<uint32_t>(SAMPLE_RATE * AppConfig::AMP_RELEASE_MS / 1000.0f);
constexpr uint32_t PITCH_DECAY_SAMPLES =
    static_cast<uint32_t>(SAMPLE_RATE * AppConfig::PITCH_DECAY_MS / 1000.0f);

int16_t outputBuffer[AudioIo::BLOCK_FRAMES * 2];
uint32_t ampSamplesRemaining = 0;
uint32_t pitchSamplesRemaining = 0;
float peakAmplitude = 0.0f;
float pitchOffsetHz = 0.0f;
float phase = 0.0f;

float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

float triangle(float oscillatorPhase) {
  if (oscillatorPhase < 0.25f) return oscillatorPhase * 4.0f;
  if (oscillatorPhase < 0.75f) return 2.0f - oscillatorPhase * 4.0f;
  return oscillatorPhase * 4.0f - 4.0f;
}

}  // namespace

void trigger(float velocity) {
  velocity = clamp01(velocity);

  const float audibleVelocity =
      AppConfig::MIN_VOLUME + (1.0f - AppConfig::MIN_VOLUME) * velocity;
  const float amplitudeVelocity =
      (1.0f - AppConfig::AMP_VELOCITY_AMOUNT) +
      AppConfig::AMP_VELOCITY_AMOUNT * audibleVelocity;
  peakAmplitude = 32767.0f * amplitudeVelocity;

  const float pitchVelocity =
      (1.0f - AppConfig::PITCH_VELOCITY_AMOUNT) +
      AppConfig::PITCH_VELOCITY_AMOUNT * velocity;
  pitchOffsetHz = AppConfig::PITCH_SWEEP_HZ * pitchVelocity;

  ampSamplesRemaining = AMP_RELEASE_SAMPLES;
  pitchSamplesRemaining = PITCH_DECAY_SAMPLES;
  phase = 0.0f;
}

const int16_t* render() {
  for (size_t frame = 0; frame < AudioIo::BLOCK_FRAMES; ++frame) {
    int16_t sample = 0;

    if (ampSamplesRemaining > 0) {
      const float ampEnvelope =
          static_cast<float>(ampSamplesRemaining) / AMP_RELEASE_SAMPLES;
      const float pitchEnvelope = pitchSamplesRemaining > 0
                                      ? static_cast<float>(pitchSamplesRemaining) /
                                            PITCH_DECAY_SAMPLES
                                      : 0.0f;
      const float frequency =
          AppConfig::BASE_FREQUENCY_HZ + pitchOffsetHz * pitchEnvelope;

      sample = static_cast<int16_t>(triangle(phase) * peakAmplitude * ampEnvelope);
      phase += frequency / SAMPLE_RATE;
      if (phase >= 1.0f) phase -= floorf(phase);

      --ampSamplesRemaining;
      if (pitchSamplesRemaining > 0) --pitchSamplesRemaining;
    }

    outputBuffer[frame * 2] = sample;
    outputBuffer[frame * 2 + 1] = sample;
  }
  return outputBuffer;
}

}  // namespace SynthVoice
