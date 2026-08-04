#pragma once

#include <stdint.h>

namespace Ads7830 {

constexpr uint8_t CHANNEL_COUNT = 8;

bool begin();
bool read(uint8_t channel, uint8_t& value);
bool readAverage(uint8_t channel, uint8_t sampleCount, float& value);

}  // namespace Ads7830
