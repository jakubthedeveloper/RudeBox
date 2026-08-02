#pragma once

#include <stdint.h>

namespace SynthVoice {

void trigger(float velocity);
const int16_t* render();

}  // namespace SynthVoice
