#include "audio_io_fake.h"

#include <stddef.h>

#include <deque>

#include "audio_io.h"

namespace {

std::deque<uint16_t> inputPeaks;
std::vector<int16_t> leftOutputSamples;
bool matchingStereoOutput = true;

}  // namespace

namespace FakeAudioIo {

void reset() {
  inputPeaks.clear();
  leftOutputSamples.clear();
  matchingStereoOutput = true;
}

void simulatePadImpulse(uint16_t peak) { inputPeaks.push_back(peak); }

const std::vector<int16_t>& writtenLeftSamples() {
  return leftOutputSamples;
}

bool stereoOutputMatches() { return matchingStereoOutput; }

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

bool write(const int16_t* stereoSamples) {
  for (size_t frame = 0; frame < BLOCK_FRAMES; ++frame) {
    const int16_t left = stereoSamples[frame * 2];
    const int16_t right = stereoSamples[frame * 2 + 1];
    leftOutputSamples.push_back(left);
    if (left != right) matchingStereoOutput = false;
  }
  return true;
}

}  // namespace AudioIo
