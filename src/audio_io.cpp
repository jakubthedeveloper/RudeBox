#include "audio_io.h"

#include <Arduino.h>
#include <driver/i2s.h>

namespace AudioIo {
namespace {

constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr size_t I2S_CHANNEL_COUNT = 2;
constexpr size_t LEFT_SLOT = 0;
constexpr size_t RIGHT_SLOT = 1;

int16_t inputBuffer[BLOCK_FRAMES * I2S_CHANNEL_COUNT];
int16_t outputBuffer[BLOCK_FRAMES * I2S_CHANNEL_COUNT];

uint16_t magnitude(int16_t sample) {
  const int32_t value = sample;
  return static_cast<uint16_t>(value < 0 ? -value : value);
}

}  // namespace

bool begin() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = BLOCK_FRAMES,
      .use_apll = true,
      .tx_desc_auto_clear = true,
      .fixed_mclk = SAMPLE_RATE * 256,
  };

  const i2s_pin_config_t pins = {
      .mck_io_num = 0,
      .bck_io_num = 27,
      .ws_io_num = 25,
      .data_out_num = 26,
      .data_in_num = 35,
  };

  if (i2s_driver_install(PORT, &config, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(PORT, &pins) == ESP_OK) return true;

  i2s_driver_uninstall(PORT);
  return false;
}

bool readMagnitudeBlock(Channel channel, MagnitudeBlock& block) {
  size_t bytesRead = 0;
  if (i2s_read(PORT, inputBuffer, sizeof(inputBuffer), &bytesRead,
               pdMS_TO_TICKS(100)) != ESP_OK) {
    return false;
  }

  const size_t frames = bytesRead / (I2S_CHANNEL_COUNT * sizeof(int16_t));
  if (frames == 0) return false;

  const size_t slot = channel == Channel::Left ? LEFT_SLOT : RIGHT_SLOT;
  block.sampleCount = frames;
  for (size_t frame = 0; frame < frames; ++frame) {
    block.samples[frame] =
        magnitude(inputBuffer[frame * I2S_CHANNEL_COUNT + slot]);
  }
  return true;
}

bool writeMono(const int16_t* samples) {
  for (size_t frame = 0; frame < BLOCK_FRAMES; ++frame) {
    outputBuffer[frame * I2S_CHANNEL_COUNT + LEFT_SLOT] = samples[frame];
    outputBuffer[frame * I2S_CHANNEL_COUNT + RIGHT_SLOT] = 0;
  }

  size_t bytesWritten = 0;
  constexpr size_t bytesToWrite = sizeof(outputBuffer);
  return i2s_write(PORT, outputBuffer, bytesToWrite, &bytesWritten,
                   pdMS_TO_TICKS(100)) == ESP_OK &&
         bytesWritten == bytesToWrite;
}

}  // namespace AudioIo
