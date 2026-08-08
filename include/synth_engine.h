#pragma once

#include "synth_controls.h"

namespace SynthEngine {

struct ProcessResult {
  bool hitDetected;
  float velocity;
};

// Processes one input block and writes one synthesized output block.
// Reports the velocity only when an accepted hit triggered the voice.
ProcessResult processAudioBlock(const SynthControls& controls);

}  // namespace SynthEngine
