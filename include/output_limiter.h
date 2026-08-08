#pragma once

#include <stdint.h>

namespace OutputLimiter {

// Applies master gain and brickwall limiting to one wide-sample mono block.
// The returned buffer remains valid until the next call.
const int16_t* process(const int32_t* inputSamples);

}  // namespace OutputLimiter
