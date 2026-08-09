#pragma once

namespace Waveforms {

// Returns a bipolar triangle wave for a phase in the [0, 1) range.
float triangle(float phase);

// Morphs triangle to square over shape [0, 0.5], then narrows the pulse over
// shape (0.5, 1]. All waveforms share the supplied phase.
float shapedPulse(float phase, float shape, float minimumPulseWidth);

}  // namespace Waveforms
