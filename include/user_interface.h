#pragma once

#include "synth_controls.h"

namespace UserInterface {

bool begin();
void indicatePadHit(float velocity);
void update();
SynthControls synthControls();

}  // namespace UserInterface
