#include "es8388.h"

#include <Arduino.h>
#include <Wire.h>

#include "app_config.h"

namespace Es8388 {
namespace {

constexpr uint8_t ADDRESS = 0x10;
constexpr int PA_ENABLE_PIN = 21;

constexpr uint8_t ADC_POWER_DOWN_LEFT_INPUT = 1U << 7;
constexpr uint8_t ADC_POWER_DOWN_RIGHT_INPUT = 1U << 6;
constexpr uint8_t ADC_POWER_DOWN_LEFT_CHANNEL = 1U << 5;
constexpr uint8_t ADC_POWER_DOWN_RIGHT_CHANNEL = 1U << 4;
constexpr uint8_t MICROPHONE_BIAS_POWER_DOWN = 1U << 3;
constexpr uint8_t ADC_INTERNAL_LOW_POWER = 1U << 0;
// Selects the codec's LINE2 pins. Some ESP32-A1S board revisions physically
// couple onboard microphones to the same pins, which cannot be undone here.
constexpr uint8_t LINE_INPUT_2_SELECTION = 0x50;

constexpr uint8_t selectedLineInputAdcPower() {
  const uint8_t unusedChannelPowerDown =
      AppConfig::AudioInput::CHANNEL == AudioIo::Channel::Right
          ? ADC_POWER_DOWN_LEFT_INPUT | ADC_POWER_DOWN_LEFT_CHANNEL
          : ADC_POWER_DOWN_RIGHT_INPUT | ADC_POWER_DOWN_RIGHT_CHANNEL;
  return unusedChannelPowerDown | MICROPHONE_BIAS_POWER_DOWN |
         ADC_INTERNAL_LOW_POWER;
}

constexpr uint8_t SELECTED_LINE_INPUT_ADC_POWER =
    selectedLineInputAdcPower();

static_assert(
    (SELECTED_LINE_INPUT_ADC_POWER & MICROPHONE_BIAS_POWER_DOWN) != 0,
    "Microphone bias must remain powered down");

TwoWire wire(1);

struct Setting {
  uint8_t reg;
  uint8_t value;
};

void setPowerAmplifierEnabled(bool enabled) {
  digitalWrite(PA_ENABLE_PIN, enabled ? HIGH : LOW);
}

void preparePowerAmplifier() {
  pinMode(PA_ENABLE_PIN, OUTPUT);
  setPowerAmplifierEnabled(false);
}

bool connectToCodec() {
  if (!wire.begin(33, 32, 400000)) return false;

  wire.beginTransmission(ADDRESS);
  return wire.endTransmission() == 0;
}

bool writeAndVerify(uint8_t reg, uint8_t value) {
  wire.beginTransmission(ADDRESS);
  wire.write(reg);
  wire.write(value);
  if (wire.endTransmission() != 0) return false;

  wire.beginTransmission(ADDRESS);
  wire.write(reg);
  if (wire.endTransmission(false) != 0 ||
      wire.requestFrom(ADDRESS, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  return wire.read() == value;
}

bool configureCodec() {
  const uint8_t gain =
      AppConfig::AudioInput::GAIN_CODE > 4 ? 4
                                           : AppConfig::AudioInput::GAIN_CODE;
  const uint8_t stereoGain = static_cast<uint8_t>((gain << 4) | gain);

  const Setting settings[] = {
      {0x19, 0x04},        // Mute the DAC during setup.
      {0x01, 0x50},        // Analog bias profile.
      {0x02, 0x00},        // Digital core and clocks on.
      {0x08, 0x00},        // Codec is the I2S slave.
      {0x04, 0xC0},        // DAC and analog outputs off.
      {0x00, 0x12},        // Play/record clock profile.
      {0x03, 0xFF},        // ADC off while it is configured.
      {0x09, stereoGain},  // Left and right PGA gain.
      {0x0A, LINE_INPUT_2_SELECTION},  // LINPUT2/RINPUT2: board LINE IN.
      {0x0B, 0x02},        // Stereo ADC, data output enabled.
      {0x0C, 0x0C},        // Standard I2S, 16-bit.
      {0x0D, 0x02},        // MCLK/Fs = 256.
      {0x0E, 0x30},        // High-pass filters on.
      {0x0F, 0x00},        // ADC unmuted.
      {0x10, 0x00},        // Left digital volume: 0 dB.
      {0x11, 0x00},        // Right digital volume: 0 dB.
      {0x12, 0x00},        // Automatic level control off.
      {0x16, 0x00},        // Noise gate off.
      {0x17, 0x18},        // DAC standard I2S, 16-bit.
      {0x18, 0x02},        // DAC MCLK/Fs = 256.
      {0x1A, 0x00},        // Left DAC digital volume: 0 dB.
      {0x26, 0x00},        // Analog bypass inputs not selected.
      {0x27, 0x90},        // Left DAC routed to left output mixer.
      {0x2A, 0x00},        // No signal routed to the right output mixer.
      {0x2B, 0x80},        // ADC and DAC share the same LRCK.
      {0x2E, 0x1E},        // LOUT1 analog volume: 0 dB.
      {0x30, 0x1E},        // LOUT2 analog volume: 0 dB.
      // Power only the selected line input and ADC; keep microphone bias off.
      {0x03, SELECTED_LINE_INPUT_ADC_POWER},
      {0x04, 0x68},        // Power the left DAC and left output drivers only.
      {0x19, 0x22},        // Unmute the DAC and preserve its default control bits.
  };

  for (const Setting& setting : settings) {
    if (!writeAndVerify(setting.reg, setting.value)) return false;
  }
  return true;
}

}  // namespace

bool begin() {
  preparePowerAmplifier();
  if (!connectToCodec()) return false;
  if (!configureCodec()) return false;

  setPowerAmplifierEnabled(true);
  return true;
}

}  // namespace Es8388
