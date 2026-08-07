#pragma once

#include "synth_controls.h"

namespace UserInterface {

bool begin();
void indicatePadHit();
void update();
SynthControls synthControls();

}  // namespace UserInterface
