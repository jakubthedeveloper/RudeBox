#pragma once

#include "audio_io.h"

namespace AppConfig {

namespace Diagnostics {

constexpr bool LOG_AUDIO_PEAKS = false;
constexpr bool LOG_TRIGGER_VALIDATION = false;

}  // namespace Diagnostics

namespace AudioInput {

constexpr AudioIo::Channel CHANNEL = AudioIo::Channel::Right;
constexpr uint8_t GAIN_CODE = 2;  // 0..4 maps to 0..12 dB in 3 dB steps.

}  // namespace AudioInput

namespace AudioOutput {

// Preserve dynamics while leaving 6 dB of digital headroom before the codec.
constexpr float MASTER_GAIN = 0.5f;

// Final sample-peak safety limiter. The ceiling is relative to full scale.
constexpr float LIMITER_CEILING = 0.8f;
constexpr float LIMITER_RELEASE_MS = 50.0f;

}  // namespace AudioOutput

namespace HitDetection {

constexpr uint16_t PAD_INPUT_MIN = 120;
constexpr uint16_t PAD_INPUT_MAX = 3000;

// Trigger-shape filter. The validation window includes the candidate sample.
constexpr uint16_t TRIGGER_PRE_THRESHOLD = 180;
constexpr uint8_t TRIGGER_VALIDATION_SAMPLES = 8;
constexpr uint8_t TRIGGER_MIN_ACTIVE_SAMPLES = 8;
constexpr uint16_t TRIGGER_FOLLOW_THRESHOLD = 100;
constexpr uint16_t TRIGGER_REARM_THRESHOLD = 120;

constexpr float VELOCITY_EFFECTIVE_MAX_AT_MIN_SENSITIVITY =
    PAD_INPUT_MAX * 1.15f;
constexpr float VELOCITY_EFFECTIVE_MAX_AT_MAX_SENSITIVITY =
    PAD_INPUT_MAX * 0.82f;
constexpr float VELOCITY_CURVE_AT_MIN_SENSITIVITY = 2.2f;
constexpr float VELOCITY_CURVE_AT_MAX_SENSITIVITY = 0.55f;

}  // namespace HitDetection

namespace Ui {

constexpr uint8_t ADS7830_ADDRESS = 0x48;
constexpr int ADS7830_SDA_PIN = 23;
constexpr int ADS7830_SCL_PIN = 18;

constexpr int SENSITIVITY_LED_PIN = 22;
constexpr uint32_t SENSITIVITY_LED_PWM_FREQUENCY_HZ = 5000;
constexpr uint8_t SENSITIVITY_LED_PWM_RESOLUTION_BITS = 8;
constexpr uint32_t SENSITIVITY_LED_UPDATE_INTERVAL_MS = 5;
constexpr uint32_t SENSITIVITY_LED_FADE_MS = 150;

}  // namespace Ui

// Potentiometer and control mapping values intended for hardware tuning.
namespace Controls {

// Potentiometer reading
constexpr uint8_t POTENTIOMETER_COUNT = 5;
constexpr uint8_t SENSITIVITY_CHANNEL = 0;
constexpr uint8_t OSC_PITCH_CHANNEL = 1;
constexpr uint8_t PITCH_DROP_CHANNEL = 2;
constexpr uint8_t CLICK_CHANNEL = 3;
constexpr uint8_t AMP_VELOCITY_CHANNEL = 4;
constexpr uint32_t CONTROL_SCAN_INTERVAL_MS = 5;
constexpr float POT_FILTER_ALPHA = 0.12f;
constexpr uint8_t POT_CHANGE_THRESHOLD = 2;
constexpr bool LOG_CONTROL_VALUES = true;
constexpr uint32_t CONTROL_LOG_INTERVAL_MS = 100;

// Oscillator pitch
constexpr float OSC_PITCH_MIN_HZ = 45.0f;
constexpr float OSC_PITCH_MAX_HZ = 1200.0f;

// Pitch envelope depth
constexpr float PITCH_DROP_MIN_OCTAVES = 0.0f;
constexpr float PITCH_DROP_MAX_OCTAVES = 4.5f;

// Values used until the first successful scan of each control.
constexpr float DEFAULT_SENSITIVITY = 0.5f;
constexpr float DEFAULT_OSC_PITCH_HZ = 150.0f;
constexpr float DEFAULT_PITCH_DROP_OCTAVES = 1.0f;
constexpr float DEFAULT_CLICK_LEVEL = 0.0f;
constexpr float DEFAULT_AMP_VELOCITY = 1.0f;

}  // namespace Controls

namespace Voice {

constexpr float AMP_RELEASE_MS = 300.0f;
constexpr float PITCH_DECAY_MS = 190.0f;
constexpr float CLICK_DECAY_MS = 8.0f;
constexpr float CLICK_MAX_AMPLITUDE = 0.25f;
constexpr float AMP_VELOCITY_FULL_SCALE = 0.9f;

constexpr float MAX_START_FREQUENCY_HZ = 8000.0f;
constexpr float PITCH_DROP_VELOCITY_MIN_SCALE = 0.65f;

}  // namespace Voice

}  // namespace AppConfig
