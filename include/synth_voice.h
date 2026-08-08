#pragma once

#include <stdint.h>

namespace SynthVoice {

void trigger(float velocity, float baseFrequencyHz, float pitchDropOctaves,
             float clickLevel, float ampVelocity);
const int32_t* render();

}  // namespace SynthVoice
