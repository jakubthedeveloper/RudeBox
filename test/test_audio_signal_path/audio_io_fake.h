#pragma once

#include <stdint.h>

#include <vector>

namespace FakeAudioIo {

void reset();
void simulatePadImpulse(uint16_t peak);
const std::vector<int16_t>& writtenLeftSamples();
bool stereoOutputMatches();

}  // namespace FakeAudioIo
