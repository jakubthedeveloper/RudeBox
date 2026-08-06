#pragma once

#include <stdint.h>

namespace SynthVoice {

void trigger(float velocity, float baseFrequencyHz, float pitchDropOctaves);
const int16_t* render();

}  // namespace SynthVoice
