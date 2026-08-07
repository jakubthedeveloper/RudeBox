#pragma once

#include "synth_controls.h"

namespace SynthEngine {

// Processes one input block and writes one synthesized output block.
// Returns true when a hit from the drum pad triggered the voice.
bool processAudioBlock(const SynthControls& controls);

}  // namespace SynthEngine
