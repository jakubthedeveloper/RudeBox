#pragma once

#include <stddef.h>
#include <stdint.h>

namespace AudioIo {

constexpr uint32_t SAMPLE_RATE = 44100;
constexpr size_t BLOCK_FRAMES = 256;

enum class Channel : uint8_t { Left, Right };

bool begin();
bool readPeak(Channel channel, uint16_t& peak);
bool write(const int16_t* stereoSamples);

}  // namespace AudioIo
