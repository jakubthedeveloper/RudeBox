#pragma once

#include <stdint.h>

namespace AudioInput {

enum class Channel : uint8_t { Left, Right };

bool begin();
bool readPeak(Channel channel, uint16_t& peak);

}  // namespace AudioInput
