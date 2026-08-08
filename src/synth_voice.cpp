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
constexpr float CLICK_SILENCE_THRESHOLD = 0.0001f;
const float CLICK_DECAY_COEFFICIENT =
    powf(CLICK_SILENCE_THRESHOLD,
         1.0f / (SAMPLE_RATE * AppConfig::Voice::CLICK_DECAY_MS / 1000.0f));
constexpr float FULL_SCALE = 32767.0f;

static_assert(AMP_RELEASE_SAMPLES > 0, "Amplitude release must not be zero");
static_assert(PITCH_DECAY_SAMPLES > 0, "Pitch decay must not be zero");
static_assert(AppConfig::Voice::CLICK_DECAY_MS > 0.0f,
              "Click decay must be positive");
static_assert(AppConfig::Voice::CLICK_MAX_AMPLITUDE >= 0.0f &&
                  AppConfig::Voice::CLICK_MAX_AMPLITUDE <= 1.0f,
              "Click amplitude must be in [0, 1]");
static_assert(AppConfig::Voice::AMP_VELOCITY_FULL_SCALE > 0.0f &&
                  AppConfig::Voice::AMP_VELOCITY_FULL_SCALE <= 1.0f,
              "Full-scale amplitude velocity must be in (0, 1]");

struct VoiceState {
  uint32_t ampSamplesRemaining = 0;
  uint32_t pitchSamplesRemaining = 0;
  float baseFrequencyHz = 0.0f;
  float pitchOffsetHz = 0.0f;
  float phase = 0.0f;
  float triggerVelocityGain = 1.0f;
  float clickVelocityScale = 1.0f;
  float clickLevel = 0.0f;
  float clickEnvelope = 0.0f;
};

int32_t outputBuffer[AudioIo::BLOCK_FRAMES];
VoiceState voice;
uint32_t noiseState = 0x6d2b79f5u;

float calculateVelocityGain(float velocity, float ampVelocity) {
  const float fullScaleVelocity = MathUtils::clamp01(
      velocity / AppConfig::Voice::AMP_VELOCITY_FULL_SCALE);
  return MathUtils::lerp(1.0f, fullScaleVelocity, ampVelocity);
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
  voice.clickEnvelope = 1.0f;
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

float generateOscillatorSample() {
  const float ampEnvelope =
      envelopeLevel(voice.ampSamplesRemaining, AMP_RELEASE_SAMPLES);
  return Waveforms::triangle(voice.phase) * ampEnvelope *
         voice.triggerVelocityGain;
}

float generateNoiseSample() {
  noiseState ^= noiseState << 13;
  noiseState ^= noiseState >> 17;
  noiseState ^= noiseState << 5;
  return static_cast<float>(noiseState) * (2.0f / 4294967295.0f) - 1.0f;
}

float generateClickSample() {
  if (voice.clickEnvelope == 0.0f || voice.clickLevel == 0.0f) return 0.0f;

  const float click = generateNoiseSample() * voice.clickEnvelope *
                      voice.clickLevel * voice.clickVelocityScale *
                      AppConfig::Voice::CLICK_MAX_AMPLITUDE;
  voice.clickEnvelope *= CLICK_DECAY_COEFFICIENT;
  if (voice.clickEnvelope < CLICK_SILENCE_THRESHOLD) {
    voice.clickEnvelope = 0.0f;
  }
  return click;
}

void advanceVoice(float frequency) {
  voice.phase += frequency / SAMPLE_RATE;
  if (voice.phase >= 1.0f) voice.phase -= floorf(voice.phase);

  --voice.ampSamplesRemaining;
  if (voice.pitchSamplesRemaining > 0) --voice.pitchSamplesRemaining;
}

int32_t renderSample() {
  if (voice.ampSamplesRemaining == 0) return 0;

  const float frequency = currentFrequency();
  const float signal = generateOscillatorSample() + generateClickSample();
  advanceVoice(frequency);
  return static_cast<int32_t>(signal * FULL_SCALE);
}

}  // namespace

void trigger(float velocity, float baseFrequencyHz, float pitchDropOctaves,
             float clickLevel, float ampVelocity) {
  const float normalizedVelocity = MathUtils::clamp01(velocity);
  voice.baseFrequencyHz = baseFrequencyHz;
  voice.pitchOffsetHz = calculatePitchOffset(
      baseFrequencyHz, pitchDropOctaves, normalizedVelocity);
  voice.triggerVelocityGain = calculateVelocityGain(
      normalizedVelocity, MathUtils::clamp01(ampVelocity));
  voice.clickVelocityScale = 0.5f + 0.5f * normalizedVelocity;
  voice.clickLevel = MathUtils::clamp01(clickLevel);
  restartVoice();
}

const int32_t* render() {
  for (size_t frame = 0; frame < AudioIo::BLOCK_FRAMES; ++frame) {
    outputBuffer[frame] = renderSample();
  }
  return outputBuffer;
}

}  // namespace SynthVoice
