#include "audio_io_fake.h"

#include <stddef.h>

#include <deque>

#include "audio_io.h"

namespace {

std::deque<std::vector<uint16_t>> inputBlocks;
std::vector<int16_t> outputSamples;

}  // namespace

namespace FakeAudioIo {

void reset() {
  inputBlocks.clear();
  outputSamples.clear();
}

void simulatePadImpulse(uint16_t peak) {
  std::vector<uint16_t> magnitudes(AudioIo::BLOCK_FRAMES, 0);
  magnitudes[0] = peak;
  magnitudes[1] = static_cast<uint16_t>(peak * 85U / 100U);
  magnitudes[2] = static_cast<uint16_t>(peak * 70U / 100U);
  magnitudes[3] = static_cast<uint16_t>(peak * 60U / 100U);
  magnitudes[4] = static_cast<uint16_t>(peak * 50U / 100U);
  magnitudes[5] = static_cast<uint16_t>(peak * 45U / 100U);
  magnitudes[6] = static_cast<uint16_t>(peak * 40U / 100U);
  magnitudes[7] = static_cast<uint16_t>(peak * 35U / 100U);
  inputBlocks.push_back(magnitudes);
}

const std::vector<int16_t>& writtenSamples() { return outputSamples; }

}  // namespace FakeAudioIo

namespace AudioIo {

bool begin() { return true; }

bool readMagnitudeBlock(Channel, MagnitudeBlock& block) {
  block.sampleCount = BLOCK_FRAMES;
  for (size_t index = 0; index < BLOCK_FRAMES; ++index) {
    block.samples[index] = 0;
  }

  if (inputBlocks.empty()) {
    return true;
  }

  const std::vector<uint16_t>& magnitudes = inputBlocks.front();
  const size_t samplesToCopy =
      magnitudes.size() < BLOCK_FRAMES ? magnitudes.size() : BLOCK_FRAMES;
  for (size_t index = 0; index < samplesToCopy; ++index) {
    block.samples[index] = magnitudes[index];
  }
  inputBlocks.pop_front();
  return true;
}

bool writeMono(const int16_t* samples) {
  outputSamples.insert(outputSamples.end(), samples, samples + BLOCK_FRAMES);
  return true;
}

}  // namespace AudioIo
