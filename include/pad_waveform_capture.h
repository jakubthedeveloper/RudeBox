#pragma once

#include <stddef.h>
#include <stdint.h>

namespace PadWaveformCapture {

// Records signed pad input blocks around a strike candidate and prints them
// over serial once the capture is complete. Printing blocks the audio loop.
void process(const int16_t* samples, size_t sampleCount,
             bool candidateStarted);

}  // namespace PadWaveformCapture
