#include "audio_io_fake.h"

#include <stddef.h>

#include <deque>

#include "audio_io.h"

namespace {

std::deque<uint16_t> inputPeaks;
std::vector<int16_t> outputSamples;

}  // namespace

namespace FakeAudioIo {

void reset() {
  inputPeaks.clear();
  outputSamples.clear();
}

void simulatePadImpulse(uint16_t peak) { inputPeaks.push_back(peak); }

const std::vector<int16_t>& writtenSamples() { return outputSamples; }

}  // namespace FakeAudioIo

namespace AudioIo {

bool begin() { return true; }

bool readPeak(Channel, uint16_t& peak) {
  if (inputPeaks.empty()) {
    peak = 0;
    return true;
  }

  peak = inputPeaks.front();
  inputPeaks.pop_front();
  return true;
}

bool writeMono(const int16_t* samples) {
  outputSamples.insert(outputSamples.end(), samples, samples + BLOCK_FRAMES);
  return true;
}

}  // namespace AudioIo
