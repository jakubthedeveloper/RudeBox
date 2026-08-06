#pragma once

#include <stdint.h>

struct SynthControls {
  float sensitivity;
  float oscPitchHz;
  float pitchDropOctaves;
};

namespace SynthControlInput {

bool begin();
void update();
SynthControls snapshot();

// Converts one accepted pad peak to the velocity consumed by the entire voice.
float velocityFromPeak(uint16_t peak, float sensitivity);

}  // namespace SynthControlInput
