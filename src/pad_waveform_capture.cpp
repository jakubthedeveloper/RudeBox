#include "pad_waveform_capture.h"

#include <Arduino.h>

#include "app_config.h"
#include "audio_io.h"

namespace PadWaveformCapture {
namespace {

constexpr size_t CAPTURE_BLOCKS =
    AppConfig::Diagnostics::PAD_WAVEFORM_CAPTURE_BLOCKS;

static_assert(CAPTURE_BLOCKS >= 2,
              "Waveform capture needs a pre-trigger and a candidate block");

struct RecordedBlock {
  int16_t samples[AudioIo::BLOCK_FRAMES];
  size_t sampleCount;
};

// Block 0 always holds the most recent block before the candidate.
RecordedBlock recordedBlocks[CAPTURE_BLOCKS];
size_t recordedBlockCount = 0;
bool capturing = false;
uint32_t captureIndex = 0;

void recordBlock(size_t slot, const int16_t* samples, size_t sampleCount);
void printCapture();

}  // namespace

void process(const int16_t* samples, size_t sampleCount,
             bool candidateStarted) {
  if (!capturing) {
    if (!candidateStarted) {
      recordBlock(0, samples, sampleCount);
      return;
    }
    capturing = true;
    recordedBlockCount = 1;
  }

  recordBlock(recordedBlockCount++, samples, sampleCount);
  if (recordedBlockCount < CAPTURE_BLOCKS) return;

  printCapture();
  capturing = false;
  recordedBlockCount = 0;
}

namespace {

void recordBlock(size_t slot, const int16_t* samples, size_t sampleCount) {
  RecordedBlock& block = recordedBlocks[slot];
  block.sampleCount =
      sampleCount < AudioIo::BLOCK_FRAMES ? sampleCount : AudioIo::BLOCK_FRAMES;
  for (size_t index = 0; index < block.sampleCount; ++index) {
    block.samples[index] = samples[index];
  }
}

void printCapture() {
  Serial.printf("#waveBegin:%lu:%u\n", static_cast<unsigned long>(captureIndex),
                static_cast<unsigned>(AudioIo::SAMPLE_RATE));
  for (size_t slot = 0; slot < CAPTURE_BLOCKS; ++slot) {
    const RecordedBlock& block = recordedBlocks[slot];
    Serial.printf("#wave:");
    for (size_t index = 0; index < block.sampleCount; ++index) {
      Serial.printf(index == 0 ? "%d" : ",%d", block.samples[index]);
    }
    Serial.printf("\n");
  }
  Serial.printf("#waveEnd:%lu\n", static_cast<unsigned long>(captureIndex));
  ++captureIndex;
}

}  // namespace
}  // namespace PadWaveformCapture
