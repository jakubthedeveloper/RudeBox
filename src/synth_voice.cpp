#include "synth_voice.h"

#include <math.h>

#include "app_config.h"
#include "audio_io.h"
#include "math_utils.h"
#include "waveforms.h"

namespace SynthVoice {
namespace {

constexpr float SAMPLE_RATE = static_cast<float>(AudioIo::SAMPLE_RATE);
constexpr float CLICK_SILENCE_THRESHOLD = 0.0001f;
const float CLICK_DECAY_COEFFICIENT =
    powf(CLICK_SILENCE_THRESHOLD,
         1.0f / (SAMPLE_RATE * AppConfig::Voice::CLICK_DECAY_MS / 1000.0f));
constexpr float FULL_SCALE = 32767.0f;

static_assert(AppConfig::Controls::DECAY_MIN_MS > 0.0f,
              "Minimum decay must be positive");
static_assert(AppConfig::Controls::DECAY_MAX_MS >=
                  AppConfig::Controls::DECAY_MIN_MS,
              "Maximum decay must not be shorter than minimum decay");
static_assert(AppConfig::Voice::MIN_PULSE_WIDTH > 0.0f &&
                  AppConfig::Voice::MIN_PULSE_WIDTH <= 0.5f,
              "Minimum pulse width must be in (0, 0.5]");
static_assert(AppConfig::Voice::CLICK_DECAY_MS > 0.0f,
              "Click decay must be positive");
static_assert(AppConfig::Voice::CLICK_MAX_AMPLITUDE >= 0.0f &&
                  AppConfig::Voice::CLICK_MAX_AMPLITUDE <= 1.0f,
              "Click amplitude must be in [0, 1]");
static_assert(AppConfig::Voice::AMP_VELOCITY_FULL_SCALE > 0.0f &&
                  AppConfig::Voice::AMP_VELOCITY_FULL_SCALE <= 1.0f,
              "Full-scale amplitude velocity must be in (0, 1]");
static_assert(AppConfig::Voice::AMP_VELOCITY_CONSTANT_GAIN > 0.0f &&
                  AppConfig::Voice::AMP_VELOCITY_CONSTANT_GAIN <= 1.0f,
              "Constant amplitude gain must be in (0, 1]");

struct VoiceState {
  uint32_t envelopeSamplesRemaining = 0;
  float baseFrequencyHz = 0.0f;
  float pitchDepthSemitones = 0.0f;
  float phase = 0.0f;
  float envelope = 0.0f;
  float envelopeDecayCoefficient = 0.0f;
  float triggerVelocityGain = 1.0f;
  float shape = 0.0f;
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
  return MathUtils::lerp(AppConfig::Voice::AMP_VELOCITY_CONSTANT_GAIN,
                         fullScaleVelocity, ampVelocity);
}

float calculatePitchDepth(float pitchDropOctaves, float envToPitchSemitones,
                          float velocity) {
  const float velocityPitchScale =
      MathUtils::lerp(AppConfig::Voice::PITCH_DROP_VELOCITY_MIN_SCALE, 1.0f,
                      velocity);
  return pitchDropOctaves * velocityPitchScale * 12.0f +
         envToPitchSemitones * velocity;
}

uint32_t decaySampleCount(float decayMs) {
  const float limitedDecayMs =
      decayMs < AppConfig::Controls::DECAY_MIN_MS
          ? AppConfig::Controls::DECAY_MIN_MS
          : (decayMs > AppConfig::Controls::DECAY_MAX_MS
                 ? AppConfig::Controls::DECAY_MAX_MS
                 : decayMs);
  return static_cast<uint32_t>(SAMPLE_RATE * limitedDecayMs / 1000.0f);
}

void restartVoice(float decayMs) {
  voice.envelopeSamplesRemaining = decaySampleCount(decayMs);
  voice.envelope = 1.0f;
  voice.envelopeDecayCoefficient = powf(
      AppConfig::Voice::ENVELOPE_SILENCE_THRESHOLD,
      1.0f / voice.envelopeSamplesRemaining);
  voice.phase = 0.0f;
  voice.clickEnvelope = 1.0f;
}

// Fourth-order exp2 approximation. This keeps transcendental functions out of
// the per-sample audio path while retaining semitone-based pitch modulation.
float fastExp2(float exponent) {
  constexpr float LN_2 = 0.69314718056f;
  const float integerPart = floorf(exponent);
  const float fraction = exponent - integerPart;
  const float x = fraction * LN_2;
  const float fractionalPower =
      1.0f + x * (1.0f + x * (0.5f + x * (1.0f / 6.0f + x / 24.0f)));
  return ldexpf(fractionalPower, static_cast<int>(integerPart));
}

float currentFrequency() {
  const float pitchOffsetSemitones =
      voice.envelope * voice.pitchDepthSemitones;
  const float frequency =
      voice.baseFrequencyHz * fastExp2(pitchOffsetSemitones / 12.0f);
  return frequency < AppConfig::Voice::MAX_START_FREQUENCY_HZ
             ? frequency
             : AppConfig::Voice::MAX_START_FREQUENCY_HZ;
}

float generateOscillatorSample() {
  return Waveforms::shapedPulse(voice.phase, voice.shape,
                                AppConfig::Voice::MIN_PULSE_WIDTH) *
         voice.envelope * voice.triggerVelocityGain;
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

  voice.envelope *= voice.envelopeDecayCoefficient;
  --voice.envelopeSamplesRemaining;
  if (voice.envelopeSamplesRemaining == 0) voice.envelope = 0.0f;
}

int32_t renderSample() {
  if (voice.envelopeSamplesRemaining == 0) return 0;

  const float frequency = currentFrequency();
  const float signal = generateOscillatorSample() + generateClickSample();
  advanceVoice(frequency);
  return static_cast<int32_t>(signal * FULL_SCALE);
}

}  // namespace

void trigger(float velocity, float baseFrequencyHz, float pitchDropOctaves,
             float clickLevel, float ampVelocity, float shapeNormalized,
             float decayMs, float envToPitchSemitones) {
  const float normalizedVelocity = MathUtils::clamp01(velocity);
  voice.baseFrequencyHz = baseFrequencyHz;
  voice.pitchDepthSemitones = calculatePitchDepth(
      pitchDropOctaves, envToPitchSemitones, normalizedVelocity);
  voice.triggerVelocityGain = calculateVelocityGain(
      normalizedVelocity, MathUtils::clamp01(ampVelocity));
  voice.clickVelocityScale = 0.5f + 0.5f * normalizedVelocity;
  voice.clickLevel = MathUtils::clamp01(clickLevel);
  voice.shape = MathUtils::clamp01(shapeNormalized);
  restartVoice(decayMs);
}

void setShape(float shapeNormalized) {
  voice.shape = MathUtils::clamp01(shapeNormalized);
}

const int32_t* render() {
  for (size_t frame = 0; frame < AudioIo::BLOCK_FRAMES; ++frame) {
    outputBuffer[frame] = renderSample();
  }
  return outputBuffer;
}

}  // namespace SynthVoice
