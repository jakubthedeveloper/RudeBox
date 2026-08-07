#include <Arduino.h>
#include <unity.h>

#include <filesystem>
#include <string>
#include <vector>

#include "app_config.h"
#include "audio_io.h"
#include "audio_io_fake.h"
#include "synth_controls.h"
#include "synth_engine.h"
#include "waveform_svg.h"

HardwareSerial Serial;

namespace {

constexpr SynthControls TEST_CONTROLS = {
    0.5f,
    150.0f,
    1.0f,
};

constexpr size_t RELEASE_SAMPLE_COUNT = static_cast<size_t>(
    AudioIo::SAMPLE_RATE * AppConfig::Voice::AMP_RELEASE_MS / 1000.0f);
constexpr size_t RENDER_BLOCK_COUNT =
    (RELEASE_SAMPLE_COUNT + AudioIo::BLOCK_FRAMES - 1) /
        AudioIo::BLOCK_FRAMES +
    1;

struct HitScenario {
  const char* name;
  const char* title;
  uint16_t inputPeak;
  int16_t minimumExpectedOutputPeak;
  int16_t maximumExpectedOutputPeak;
};

struct RenderResult {
  bool hitDetected;
  bool unexpectedRetrigger;
  std::vector<int16_t> outputSamples;
};

std::filesystem::path artifactDirectory() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() /
         "artifacts";
}

std::filesystem::path artifactPath(const HitScenario& scenario) {
  return artifactDirectory() /
         ("audio_path_" + std::string(scenario.name) + "_pad_hit_peak_" +
          std::to_string(scenario.inputPeak) + ".svg");
}

RenderResult renderPadHit(const HitScenario& scenario) {
  // A quiet input block establishes the real detector's armed baseline and
  // makes every scenario independent from the preceding test.
  FakeAudioIo::reset();
  SynthEngine::processAudioBlock(TEST_CONTROLS);

  FakeAudioIo::reset();
  FakeAudioIo::simulatePadImpulse(scenario.inputPeak);

  const bool hitDetected = SynthEngine::processAudioBlock(TEST_CONTROLS);
  bool unexpectedRetrigger = false;
  for (size_t block = 1; block < RENDER_BLOCK_COUNT; ++block) {
    if (SynthEngine::processAudioBlock(TEST_CONTROLS)) {
      unexpectedRetrigger = true;
    }
  }

  return {
      hitDetected,
      unexpectedRetrigger,
      FakeAudioIo::writtenSamples(),
  };
}

int16_t findOutputPeak(const std::vector<int16_t>& samples) {
  int32_t peak = 0;
  for (const int16_t sample : samples) {
    const int32_t magnitude =
        sample < 0 ? -static_cast<int32_t>(sample) : sample;
    if (magnitude > peak) peak = magnitude;
  }
  return static_cast<int16_t>(peak);
}

void writeArtifact(const HitScenario& scenario,
                   const std::vector<int16_t>& samples) {
  const WaveformSvg::PlotDescription description = {
      scenario.title,
      scenario.inputPeak,
      TEST_CONTROLS.sensitivity,
      TEST_CONTROLS.oscPitchHz,
      TEST_CONTROLS.pitchDropOctaves,
      AudioIo::SAMPLE_RATE,
  };
  TEST_ASSERT_TRUE_MESSAGE(
      WaveformSvg::write(artifactPath(scenario), samples, description),
      "Could not write the waveform SVG artifact");
}

void verifyAudioPath(const HitScenario& scenario) {
  const RenderResult result = renderPadHit(scenario);
  writeArtifact(scenario, result.outputSamples);

  TEST_ASSERT_TRUE_MESSAGE(result.hitDetected,
                           "The simulated pad impulse did not trigger a hit");
  TEST_ASSERT_FALSE_MESSAGE(
      result.unexpectedRetrigger,
      "One pad impulse triggered the voice more than once");
  TEST_ASSERT_EQUAL_UINT32(RENDER_BLOCK_COUNT * AudioIo::BLOCK_FRAMES,
                           result.outputSamples.size());
  TEST_ASSERT_EQUAL_INT16(0, result.outputSamples.back());

  const int16_t outputPeak = findOutputPeak(result.outputSamples);
  TEST_ASSERT_TRUE_MESSAGE(
      outputPeak >= scenario.minimumExpectedOutputPeak,
      "Output peak is lower than expected for this hit strength");
  TEST_ASSERT_TRUE_MESSAGE(
      outputPeak <= scenario.maximumExpectedOutputPeak,
      "Output peak is higher than expected for this hit strength");
}

void testWeakPadHit() {
  verifyAudioPath({"weak", "Audio path - weak pad hit", 300, 1000, 1250});
}

void testMediumPadHit() {
  verifyAudioPath(
      {"medium", "Audio path - medium pad hit", 1560, 6500, 7200});
}

void testMaximumPadHit() {
  verifyAudioPath(
      {"maximum", "Audio path - maximum pad hit", 3000, 15500, 16500});
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testWeakPadHit);
  RUN_TEST(testMediumPadHit);
  RUN_TEST(testMaximumPadHit);
  return UNITY_END();
}
