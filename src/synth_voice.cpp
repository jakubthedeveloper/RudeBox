#include "synth_voice.h"

#include <math.h>

#include "app_config.h"
#include "audio_io.h"
#include "math_utils.h"
#include "waveforms.h"

namespace SynthVoice {
namespace {

constexpr float SAMPLE_RATE = static_cast<float>(AudioIo::SAMPLE_RATE);
constexpr uint32_t AMP_RELEASE_SAMPLES =
    static_cast<uint32_t>(SAMPLE_RATE * AppConfig::Voice::AMP_RELEASE_MS /
                          1000.0f);
constexpr uint32_t PITCH_DECAY_SAMPLES =
    static_cast<uint32_t>(SAMPLE_RATE * AppConfig::Voice::PITCH_DECAY_MS /
                          1000.0f);

static_assert(AMP_RELEASE_SAMPLES > 0, "Amplitude release must not be zero");
static_assert(PITCH_DECAY_SAMPLES > 0, "Pitch decay must not be zero");

struct VoiceState {
  uint32_t ampSamplesRemaining = 0;
  uint32_t pitchSamplesRemaining = 0;
  float peakAmplitude = 0.0f;
  float baseFrequencyHz = 0.0f;
  float pitchOffsetHz = 0.0f;
  float phase = 0.0f;
};

int16_t outputBuffer[AudioIo::BLOCK_FRAMES * 2];
VoiceState voice;

float calculatePeakAmplitude(float velocity) {
  const float audibleVelocity =
      AppConfig::Voice::MIN_VOLUME +
      (1.0f - AppConfig::Voice::MIN_VOLUME) * velocity;
  const float amplitudeVelocity =
      (1.0f - AppConfig::Voice::AMP_VELOCITY_AMOUNT) +
      AppConfig::Voice::AMP_VELOCITY_AMOUNT * audibleVelocity;
  return 32767.0f * amplitudeVelocity;
}

float calculatePitchOffset(float baseFrequencyHz, float pitchDropOctaves,
                           float velocity) {
  const float velocityPitchScale =
      MathUtils::lerp(AppConfig::Voice::PITCH_DROP_VELOCITY_MIN_SCALE, 1.0f,
                      velocity);
  const float effectivePitchDrop = pitchDropOctaves * velocityPitchScale;
  const float startFrequency =
      baseFrequencyHz * powf(2.0f, effectivePitchDrop);
  const float limitedStartFrequency =
      startFrequency < AppConfig::Voice::MAX_START_FREQUENCY_HZ
          ? startFrequency
          : AppConfig::Voice::MAX_START_FREQUENCY_HZ;
  return limitedStartFrequency - baseFrequencyHz;
}

void restartVoice() {
  voice.ampSamplesRemaining = AMP_RELEASE_SAMPLES;
  voice.pitchSamplesRemaining = PITCH_DECAY_SAMPLES;
  voice.phase = 0.0f;
}

float envelopeLevel(uint32_t samplesRemaining, uint32_t totalSamples) {
  return static_cast<float>(samplesRemaining) / totalSamples;
}

float currentFrequency() {
  const float pitchEnvelope =
      voice.pitchSamplesRemaining > 0
          ? envelopeLevel(voice.pitchSamplesRemaining, PITCH_DECAY_SAMPLES)
          : 0.0f;
  return voice.baseFrequencyHz + voice.pitchOffsetHz * pitchEnvelope;
}

int16_t generateCurrentSample() {
  const float ampEnvelope =
      envelopeLevel(voice.ampSamplesRemaining, AMP_RELEASE_SAMPLES);
  return static_cast<int16_t>(Waveforms::triangle(voice.phase) *
                              voice.peakAmplitude * ampEnvelope);
}

void advanceVoice(float frequency) {
  voice.phase += frequency / SAMPLE_RATE;
  if (voice.phase >= 1.0f) voice.phase -= floorf(voice.phase);

  --voice.ampSamplesRemaining;
  if (voice.pitchSamplesRemaining > 0) --voice.pitchSamplesRemaining;
}

int16_t renderSample() {
  if (voice.ampSamplesRemaining == 0) return 0;

  const float frequency = currentFrequency();
  const int16_t sample = generateCurrentSample();
  advanceVoice(frequency);
  return sample;
}

void writeStereoFrame(size_t frame, int16_t sample) {
  outputBuffer[frame * 2] = sample;
  outputBuffer[frame * 2 + 1] = sample;
}

}  // namespace

void trigger(float velocity, float baseFrequencyHz, float pitchDropOctaves) {
  const float normalizedVelocity = MathUtils::clamp01(velocity);
  voice.peakAmplitude = calculatePeakAmplitude(normalizedVelocity);
  voice.baseFrequencyHz = baseFrequencyHz;
  voice.pitchOffsetHz = calculatePitchOffset(
      baseFrequencyHz, pitchDropOctaves, normalizedVelocity);
  restartVoice();
}

const int16_t* render() {
  for (size_t frame = 0; frame < AudioIo::BLOCK_FRAMES; ++frame) {
    writeStereoFrame(frame, renderSample());
  }
  return outputBuffer;
}

}  // namespace SynthVoice
