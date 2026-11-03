#pragma once

#include <stddef.h>
#include <stdint.h>

namespace AudioIo {

constexpr uint32_t SAMPLE_RATE = 44100;
constexpr size_t BLOCK_FRAMES = 256;

enum class Channel : uint8_t { Left, Right };

struct SampleBlock {
  int16_t samples[BLOCK_FRAMES];
  size_t sampleCount;
};

bool begin();
bool readSampleBlock(Channel channel, SampleBlock& block);

// Writes one mono block to the left audio output. The right output stays silent.
bool writeMono(const int16_t* samples);

}  // namespace AudioIo
