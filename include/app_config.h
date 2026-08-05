#pragma once

#include "audio_io.h"

namespace AppConfig {

namespace Diagnostics {

constexpr bool LOG_AUDIO_PEAKS = false;

}  // namespace Diagnostics

namespace AudioInput {

constexpr AudioIo::Channel CHANNEL = AudioIo::Channel::Right;
constexpr uint8_t GAIN_CODE = 2;  // 0..4 maps to 0..12 dB in 3 dB steps.

}  // namespace AudioInput

namespace HitDetection {

constexpr uint16_t MIN_HIT_PEAK = 120;
constexpr uint16_t MAX_HIT_PEAK = 3000;

}  // namespace HitDetection

namespace Ui {

constexpr uint8_t ADS7830_ADDRESS = 0x48;
constexpr int ADS7830_SDA_PIN = 23;
constexpr int ADS7830_SCL_PIN = 18;
constexpr uint8_t POTENTIOMETER_COUNT = 1;
constexpr uint8_t POTENTIOMETER_SAMPLES_PER_READING = 16;
constexpr uint32_t POTENTIOMETER_REPORT_INTERVAL_MS = 1000;
constexpr bool LOG_POTENTIOMETERS = true;

constexpr int ACTIVITY_LED_PIN = 22;
constexpr bool ACTIVITY_LED_ACTIVE_LOW = true;
constexpr uint32_t ACTIVITY_LED_PULSE_MS = 40;

}  // namespace Ui

namespace Voice {

constexpr float BASE_FREQUENCY_HZ = 120.0f;
constexpr float PITCH_SWEEP_HZ = 220.0f;
constexpr float AMP_RELEASE_MS = 300.0f;
constexpr float PITCH_DECAY_MS = 190.0f;

constexpr float MIN_VOLUME = 0.05f;
constexpr float AMP_VELOCITY_AMOUNT = 1.0f;
constexpr float PITCH_VELOCITY_AMOUNT = 0.7f;

}  // namespace Voice

}  // namespace AppConfig
