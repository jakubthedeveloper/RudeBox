#include "audio_io_fake.h"

#include <stddef.h>

#include <deque>

#include "app_config.h"
#include "audio_io.h"

namespace {

std::deque<std::vector<int16_t>> inputBlocks;
std::vector<int16_t> outputSamples;

std::vector<int16_t> decayingPadImpulse(uint16_t peak) {
  std::vector<int16_t> samples(AudioIo::BLOCK_FRAMES, 0);
  const size_t lastSample =
      AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES - 1;
  for (size_t sample = 0; sample <= lastSample; ++sample) {
    const uint32_t percentage = 100U - 75U * sample / lastSample;
    samples[sample] =
        static_cast<int16_t>(static_cast<uint32_t>(peak) * percentage / 100U);
  }
  return samples;
}

}  // namespace

namespace FakeAudioIo {

void reset() {
  inputBlocks.clear();
  outputSamples.clear();
}

void simulatePadImpulse(uint16_t peak) {
  inputBlocks.push_back(decayingPadImpulse(peak));
}

void simulateInvertedPadImpulse(uint16_t peak) {
  std::vector<int16_t> samples = decayingPadImpulse(peak);
  for (int16_t& sample : samples) sample = static_cast<int16_t>(-sample);
  inputBlocks.push_back(samples);
}

void simulateRisingPadImpulse(uint16_t peak) {
  std::vector<int16_t> samples(AudioIo::BLOCK_FRAMES, 0);
  constexpr size_t PEAK_SAMPLE = 16;
  const size_t lastSample =
      AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES - 1;
  const uint16_t tail =
      peak / 4U > AppConfig::HitDetection::TRIGGER_FOLLOW_THRESHOLD
          ? peak / 4U
          : AppConfig::HitDetection::TRIGGER_FOLLOW_THRESHOLD;
  for (size_t sample = 0; sample <= PEAK_SAMPLE; ++sample) {
    samples[sample] = static_cast<int16_t>(
        AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD +
        (static_cast<uint32_t>(peak) -
         AppConfig::HitDetection::TRIGGER_PRE_THRESHOLD) *
            sample / PEAK_SAMPLE);
  }
  for (size_t sample = PEAK_SAMPLE + 1; sample <= lastSample; ++sample) {
    samples[sample] = static_cast<int16_t>(
        peak - (static_cast<uint32_t>(peak) - tail) *
                   (sample - PEAK_SAMPLE) / (lastSample - PEAK_SAMPLE));
  }
  inputBlocks.push_back(samples);
}

const std::vector<int16_t>& writtenSamples() { return outputSamples; }

}  // namespace FakeAudioIo

namespace AudioIo {

bool begin() { return true; }

bool readSampleBlock(Channel, SampleBlock& block) {
  block.sampleCount = BLOCK_FRAMES;
  for (size_t index = 0; index < BLOCK_FRAMES; ++index) {
    block.samples[index] = 0;
  }

  if (inputBlocks.empty()) {
    return true;
  }

  const std::vector<int16_t>& samples = inputBlocks.front();
  const size_t samplesToCopy =
      samples.size() < BLOCK_FRAMES ? samples.size() : BLOCK_FRAMES;
  for (size_t index = 0; index < samplesToCopy; ++index) {
    block.samples[index] = samples[index];
  }
  inputBlocks.pop_front();
  return true;
}

bool writeMono(const int16_t* samples) {
  outputSamples.insert(outputSamples.end(), samples, samples + BLOCK_FRAMES);
  return true;
}

}  // namespace AudioIo
