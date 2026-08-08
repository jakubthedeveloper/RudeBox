#include <Arduino.h>
#include <unity.h>

#include <filesystem>
#include <string>
#include <vector>

#include "app_config.h"
#include "audio_io.h"
#include "audio_io_fake.h"
#include "hit_detector.h"
#include "synth_controls.h"
#include "synth_engine.h"
#include "synth_voice.h"
#include "waveform_svg.h"

HardwareSerial Serial;

namespace {

constexpr SynthControls TEST_CONTROLS = {
    0.5f,
    150.0f,
    1.0f,
    0.0f,
    1.0f,
};

constexpr SynthControls MIXED_CLICK_CONTROLS = {
    0.5f,
    150.0f,
    1.0f,
    1.0f,
    0.0f,
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

void resetHitDetector(float sensitivity = TEST_CONTROLS.sensitivity) {
  const std::vector<uint16_t> quietBlock(AudioIo::BLOCK_FRAMES, 0);
  HitDetector::process(quietBlock.data(), quietBlock.size(), sensitivity);
}

std::filesystem::path artifactDirectory() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() /
         "artifacts";
}

std::filesystem::path artifactPath(const HitScenario& scenario) {
  return artifactDirectory() /
         ("audio_path_" + std::string(scenario.name) + "_pad_hit_peak_" +
          std::to_string(scenario.inputPeak) + ".svg");
}

RenderResult renderPadHit(
    const HitScenario& scenario,
    const SynthControls& controls = TEST_CONTROLS) {
  // A quiet input block establishes the real detector's armed baseline and
  // makes every scenario independent from the preceding test.
  resetHitDetector(controls.sensitivity);
  FakeAudioIo::reset();
  FakeAudioIo::simulatePadImpulse(scenario.inputPeak);

  const bool hitDetected = SynthEngine::processAudioBlock(controls);
  bool unexpectedRetrigger = false;
  for (size_t block = 1; block < RENDER_BLOCK_COUNT; ++block) {
    if (SynthEngine::processAudioBlock(controls)) {
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
                   const std::vector<int16_t>& samples,
                   const SynthControls& controls = TEST_CONTROLS) {
  const WaveformSvg::PlotDescription description = {
      scenario.title,
      scenario.inputPeak,
      controls.sensitivity,
      controls.oscPitchHz,
      controls.pitchDropOctaves,
      controls.clickLevel,
      controls.ampVelocity,
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
  verifyAudioPath({"weak", "Audio path - weak pad hit", 300, 300, 450});
}

void testMediumPadHit() {
  verifyAudioPath(
      {"medium", "Audio path - medium pad hit", 1560, 6900, 7300});
}

void testMaximumPadHit() {
  verifyAudioPath(
      {"maximum", "Audio path - maximum pad hit", 3000, 15500, 16500});
}

void testMixedClickAndVoiceArtifact() {
  constexpr float ARTIFACT_DURATION_MS = 20.0f;
  const HitScenario scenario = {
      "mixed_click_and_voice",
      "Audio path - click and voice mix",
      1560,
      0,
      32767,
  };
  const RenderResult result = renderPadHit(scenario, MIXED_CLICK_CONTROLS);
  const size_t artifactSampleCount = static_cast<size_t>(
      AudioIo::SAMPLE_RATE * ARTIFACT_DURATION_MS / 1000.0f);

  TEST_ASSERT_TRUE_MESSAGE(result.hitDetected,
                           "The mixed click scenario did not trigger a hit");
  TEST_ASSERT_FALSE_MESSAGE(result.unexpectedRetrigger,
                            "The mixed click scenario retriggered unexpectedly");
  TEST_ASSERT_TRUE(result.outputSamples.size() >= artifactSampleCount);

  const std::vector<int16_t> closeUp(
      result.outputSamples.begin(),
      result.outputSamples.begin() + artifactSampleCount);
  writeArtifact(scenario, closeUp, MIXED_CLICK_CONTROLS);
}

void testSingleSampleElectricalSpikeIsRejected() {
  resetHitDetector();
  std::vector<uint16_t> magnitudes(AudioIo::BLOCK_FRAMES, 0);
  magnitudes[20] = 3000;

  const HitDetector::Result result = HitDetector::process(
      magnitudes.data(), magnitudes.size(), TEST_CONTROLS.sensitivity);

  TEST_ASSERT_TRUE(result.candidateStarted);
  TEST_ASSERT_TRUE(result.validationCompleted);
  TEST_ASSERT_FALSE(result.hitDetected);
  TEST_ASSERT_EQUAL_UINT16(3000, result.validationMax);
  TEST_ASSERT_EQUAL_UINT8(1, result.activeSamples);
  TEST_ASSERT_EQUAL_UINT32(3000, result.windowEnergy);
}

void testLightPadTailIsAccepted() {
  resetHitDetector();
  std::vector<uint16_t> magnitudes(AudioIo::BLOCK_FRAMES, 0);
  magnitudes[20] = 180;
  magnitudes[21] = 420;
  magnitudes[22] = 300;
  magnitudes[23] = 250;
  magnitudes[24] = 200;
  magnitudes[25] = 170;
  magnitudes[26] = 140;
  magnitudes[27] = 110;

  const HitDetector::Result result = HitDetector::process(
      magnitudes.data(), magnitudes.size(), TEST_CONTROLS.sensitivity);

  TEST_ASSERT_TRUE(result.candidateStarted);
  TEST_ASSERT_TRUE(result.validationCompleted);
  TEST_ASSERT_TRUE(result.hitDetected);
  TEST_ASSERT_EQUAL_UINT16(420, result.validationMax);
  TEST_ASSERT_EQUAL_UINT8(8, result.activeSamples);
  TEST_ASSERT_EQUAL_UINT32(1770, result.windowEnergy);
}

void testValidationContinuesAcrossInputBlocks() {
  resetHitDetector();
  std::vector<uint16_t> firstBlock(AudioIo::BLOCK_FRAMES, 0);
  firstBlock.back() = 420;

  const HitDetector::Result firstResult = HitDetector::process(
      firstBlock.data(), firstBlock.size(), TEST_CONTROLS.sensitivity);
  TEST_ASSERT_TRUE(firstResult.candidateStarted);
  TEST_ASSERT_FALSE(firstResult.validationCompleted);
  TEST_ASSERT_FALSE(firstResult.hitDetected);

  std::vector<uint16_t> secondBlock(AudioIo::BLOCK_FRAMES, 0);
  secondBlock[0] = 350;
  secondBlock[1] = 300;
  secondBlock[2] = 250;
  secondBlock[3] = 200;
  secondBlock[4] = 170;
  secondBlock[5] = 140;
  secondBlock[6] = 110;
  const HitDetector::Result secondResult = HitDetector::process(
      secondBlock.data(), secondBlock.size(), TEST_CONTROLS.sensitivity);

  TEST_ASSERT_TRUE(secondResult.validationCompleted);
  TEST_ASSERT_TRUE(secondResult.hitDetected);
  TEST_ASSERT_EQUAL_UINT16(420, secondResult.validationMax);
  TEST_ASSERT_EQUAL_UINT8(8, secondResult.activeSamples);
}

void testAcceptedHitStaysLockedOutUntilQuietBlock() {
  resetHitDetector();
  FakeAudioIo::reset();
  FakeAudioIo::simulatePadImpulse(600);
  TEST_ASSERT_TRUE(SynthEngine::processAudioBlock(TEST_CONTROLS));

  FakeAudioIo::simulatePadImpulse(500);
  TEST_ASSERT_FALSE(SynthEngine::processAudioBlock(TEST_CONTROLS));
  TEST_ASSERT_FALSE(SynthEngine::processAudioBlock(TEST_CONTROLS));

  FakeAudioIo::simulatePadImpulse(500);
  TEST_ASSERT_TRUE(SynthEngine::processAudioBlock(TEST_CONTROLS));
}

bool blockContainsNonZeroSample(const int32_t* samples) {
  for (size_t sample = 0; sample < AudioIo::BLOCK_FRAMES; ++sample) {
    if (samples[sample] != 0) return true;
  }
  return false;
}

std::vector<int16_t> renderClickEnvelope() {
  constexpr float ARTIFACT_DURATION_MS =
      AppConfig::Voice::CLICK_DECAY_MS + 4.0f;
  const size_t sampleCount = static_cast<size_t>(
      AudioIo::SAMPLE_RATE * ARTIFACT_DURATION_MS / 1000.0f);
  const size_t blockCount =
      (sampleCount + AudioIo::BLOCK_FRAMES - 1) / AudioIo::BLOCK_FRAMES;

  SynthVoice::trigger(1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
  std::vector<int16_t> samples;
  samples.reserve(sampleCount);
  for (size_t block = 0; block < blockCount; ++block) {
    const int32_t* rendered = SynthVoice::render();
    for (size_t frame = 0;
         frame < AudioIo::BLOCK_FRAMES && samples.size() < sampleCount;
         ++frame) {
      samples.push_back(static_cast<int16_t>(rendered[frame]));
    }
  }
  return samples;
}

bool samplesContainNonZero(const std::vector<int16_t>& samples,
                           size_t begin, size_t end) {
  for (size_t sample = begin; sample < end; ++sample) {
    if (samples[sample] != 0) return true;
  }
  return false;
}

void writeClickEnvelopeArtifact(const std::vector<int16_t>& samples) {
  const WaveformSvg::PlotDescription description = {
      "Click envelope - maximum level",
      0,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      AudioIo::SAMPLE_RATE,
  };
  TEST_ASSERT_TRUE_MESSAGE(
      WaveformSvg::write(artifactDirectory() /
                             "audio_path_click_envelope_level_1.svg",
                         samples, description),
      "Could not write the click envelope SVG artifact");
}

void testClickIsSilentAtMinimumLevel() {
  SynthVoice::trigger(1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  TEST_ASSERT_FALSE(blockContainsNonZeroSample(SynthVoice::render()));
}

void testClickDecaysAndCanBeRetriggered() {
  const std::vector<int16_t> samples = renderClickEnvelope();
  const size_t clickDecaySamples = static_cast<size_t>(
      AudioIo::SAMPLE_RATE * AppConfig::Voice::CLICK_DECAY_MS / 1000.0f);

  TEST_ASSERT_TRUE(samplesContainNonZero(
      samples, 0, AudioIo::BLOCK_FRAMES));
  TEST_ASSERT_FALSE(samplesContainNonZero(
      samples, clickDecaySamples + 1, samples.size()));
  writeClickEnvelopeArtifact(samples);

  SynthVoice::trigger(0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
  TEST_ASSERT_TRUE(blockContainsNonZeroSample(SynthVoice::render()));
}

void testAmpVelocityInterpolatesBetweenConstantAndFullDynamics() {
  constexpr float VELOCITY = 0.2f;
  constexpr float QUARTER_SAMPLE_RATE = AudioIo::SAMPLE_RATE / 4.0f;

  SynthVoice::trigger(VELOCITY, QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 0.0f);
  const int32_t constantAmplitude = SynthVoice::render()[1];

  SynthVoice::trigger(VELOCITY, QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 1.0f);
  const int32_t velocityAmplitude = SynthVoice::render()[1];

  TEST_ASSERT_TRUE(constantAmplitude > velocityAmplitude * 4.4f);
  TEST_ASSERT_TRUE(constantAmplitude < velocityAmplitude * 4.6f);
}

void testStrongHitLevelMatchesConstantAmplitude() {
  constexpr float QUARTER_SAMPLE_RATE = AudioIo::SAMPLE_RATE / 4.0f;

  SynthVoice::trigger(AppConfig::Voice::AMP_VELOCITY_FULL_SCALE,
                      QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 0.0f);
  const int32_t constantAmplitude = SynthVoice::render()[1];

  SynthVoice::trigger(AppConfig::Voice::AMP_VELOCITY_FULL_SCALE,
                      QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 1.0f);
  const int32_t fullVelocityAmplitude = SynthVoice::render()[1];

  TEST_ASSERT_EQUAL_INT32(constantAmplitude, fullVelocityAmplitude);
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testSingleSampleElectricalSpikeIsRejected);
  RUN_TEST(testLightPadTailIsAccepted);
  RUN_TEST(testValidationContinuesAcrossInputBlocks);
  RUN_TEST(testAcceptedHitStaysLockedOutUntilQuietBlock);
  RUN_TEST(testClickIsSilentAtMinimumLevel);
  RUN_TEST(testClickDecaysAndCanBeRetriggered);
  RUN_TEST(testAmpVelocityInterpolatesBetweenConstantAndFullDynamics);
  RUN_TEST(testStrongHitLevelMatchesConstantAmplitude);
  RUN_TEST(testWeakPadHit);
  RUN_TEST(testMediumPadHit);
  RUN_TEST(testMaximumPadHit);
  RUN_TEST(testMixedClickAndVoiceArtifact);
  return UNITY_END();
}
