#include "output_limiter.h"

#include "app_config.h"
#include "audio_io.h"

namespace OutputLimiter {
namespace {

constexpr size_t SAMPLE_COUNT = AudioIo::BLOCK_FRAMES;
constexpr float FULL_SCALE = 32767.0f;
constexpr int32_t CEILING_SAMPLE = static_cast<int32_t>(
    FULL_SCALE * AppConfig::AudioOutput::LIMITER_CEILING);
constexpr float BLOCK_DURATION_MS =
    1000.0f * AudioIo::BLOCK_FRAMES / AudioIo::SAMPLE_RATE;

static_assert(AppConfig::AudioOutput::MASTER_GAIN > 0.0f &&
                  AppConfig::AudioOutput::MASTER_GAIN <= 1.0f,
              "Output master gain must be in (0, 1]");
static_assert(AppConfig::AudioOutput::LIMITER_CEILING > 0.0f &&
                  AppConfig::AudioOutput::LIMITER_CEILING <= 1.0f,
              "Limiter ceiling must be in (0, 1]");
static_assert(AppConfig::AudioOutput::LIMITER_RELEASE_MS > 0.0f,
              "Limiter release time must be positive");

int16_t outputBuffer[SAMPLE_COUNT];
float limiterGain = 1.0f;

uint32_t magnitude(int16_t sample) {
  const int32_t value = sample;
  return static_cast<uint32_t>(value < 0 ? -value : value);
}

uint32_t findBlockPeak(const int16_t* inputSamples) {
  uint32_t peak = 0;
  for (size_t sample = 0; sample < SAMPLE_COUNT; ++sample) {
    const uint32_t value = magnitude(inputSamples[sample]);
    if (value > peak) peak = value;
  }
  return peak;
}

float requiredLimiterGain(uint32_t peak) {
  const float peakAfterMaster =
      peak * AppConfig::AudioOutput::MASTER_GAIN;
  if (peakAfterMaster <= CEILING_SAMPLE) return 1.0f;
  return CEILING_SAMPLE / peakAfterMaster;
}

void updateLimiterGain(float requiredGain) {
  if (requiredGain < limiterGain) {
    // The full block is available, so gain can attack before its first sample.
    limiterGain = requiredGain;
    return;
  }

  float releaseStep =
      BLOCK_DURATION_MS / AppConfig::AudioOutput::LIMITER_RELEASE_MS;
  if (releaseStep > 1.0f) releaseStep = 1.0f;
  const float releasedGain =
      limiterGain + (1.0f - limiterGain) * releaseStep;
  limiterGain = releasedGain < requiredGain ? releasedGain : requiredGain;
}

int16_t limitSample(int16_t inputSample, float totalGain) {
  int32_t sample = static_cast<int32_t>(inputSample * totalGain);
  if (sample > CEILING_SAMPLE) sample = CEILING_SAMPLE;
  if (sample < -CEILING_SAMPLE) sample = -CEILING_SAMPLE;
  return static_cast<int16_t>(sample);
}

}  // namespace

const int16_t* process(const int16_t* inputSamples) {
  const uint32_t peak = findBlockPeak(inputSamples);
  updateLimiterGain(requiredLimiterGain(peak));

  const float totalGain =
      AppConfig::AudioOutput::MASTER_GAIN * limiterGain;
  for (size_t sample = 0; sample < SAMPLE_COUNT; ++sample) {
    outputBuffer[sample] = limitSample(inputSamples[sample], totalGain);
  }
  return outputBuffer;
}

}  // namespace OutputLimiter
