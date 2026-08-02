#pragma once

#include "audio_io.h"

namespace AppConfig {

constexpr AudioIo::Channel INPUT_CHANNEL = AudioIo::Channel::Right;
constexpr uint8_t INPUT_GAIN_CODE = 2;  // 0..4 maps to 0..12 dB in 3 dB steps.

constexpr uint16_t MIN_HIT_PEAK = 120;
constexpr uint16_t MAX_HIT_PEAK = 3000;

constexpr float BASE_FREQUENCY_HZ = 120.0f;
constexpr float PITCH_SWEEP_HZ = 220.0f;
constexpr float AMP_RELEASE_MS = 300.0f;
constexpr float PITCH_DECAY_MS = 190.0f;

constexpr float MIN_VOLUME = 0.05f;
constexpr float AMP_VELOCITY_AMOUNT = 1.0f;
constexpr float PITCH_VELOCITY_AMOUNT = 0.7f;

}  // namespace AppConfig
