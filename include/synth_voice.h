#pragma once

#include <stdint.h>

namespace SynthVoice {

void trigger(float velocity, float baseFrequencyHz, float pitchDropOctaves,
             float clickLevel, float ampVelocity, float shapeNormalized,
             float decayMs, float envToPitchSemitones);
void setShape(float shapeNormalized);
const int32_t* render();

}  // namespace SynthVoice
