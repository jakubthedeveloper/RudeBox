#pragma once

#include <stdint.h>

#include <vector>

namespace FakeAudioIo {

void reset();
void simulatePadImpulse(uint16_t peak);
const std::vector<int16_t>& writtenSamples();

}  // namespace FakeAudioIo
