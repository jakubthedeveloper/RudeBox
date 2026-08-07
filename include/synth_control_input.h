#pragma once

#include "synth_controls.h"

namespace SynthControlInput {

bool begin();
void update();
SynthControls snapshot();

}  // namespace SynthControlInput
