#include <Arduino.h>
#include <unity.h>

#include <filesystem>
#include <string>
#include <vector>

#include "app_config.h"
#include "audio_io.h"
#include "audio_io_fake.h"
#include "hit_detector.h"
#include "sensitivity_svg.h"
#include "synth_controls.h"
#include "synth_engine.h"
#include "synth_voice.h"
#include "waveforms.h"
#include "waveform_svg.h"

HardwareSerial Serial;

namespace {

constexpr SynthControls TEST_CONTROLS = {
    0.5f,
    150.0f,
    1.0f,
    0.0f,
    1.0f,
    0.0f,
    300.0f,
    0.0f,
};

constexpr SynthControls MIXED_CLICK_CONTROLS = {
    0.5f,
    150.0f,
    1.0f,
    1.0f,
    0.0f,
    0.0f,
    300.0f,
    0.0f,
};

constexpr uint16_t WEAK_RISING_PEAK =
    AppConfig::HitDetection::PAD_INPUT_MIN +
    (AppConfig::HitDetection::PAD_INPUT_MAX -
     AppConfig::HitDetection::PAD_INPUT_MIN) /
        50;
constexpr uint16_t STRONG_RISING_PEAK =
    AppConfig::HitDetection::PAD_INPUT_MIN +
    (AppConfig::HitDetection::PAD_INPUT_MAX -
     AppConfig::HitDetection::PAD_INPUT_MIN) *
        9 / 10;

size_t renderBlockCount(float decayMs) {
  const size_t releaseSampleCount = static_cast<size_t>(
      AudioIo::SAMPLE_RATE * decayMs / 1000.0f);
  return (releaseSampleCount + AudioIo::BLOCK_FRAMES - 1) /
             AudioIo::BLOCK_FRAMES +
         1;
}

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

struct VelocityRenderResult {
  float velocity;
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

  const bool hitDetected =
      SynthEngine::processAudioBlock(controls).hitDetected;
  bool unexpectedRetrigger = false;
  const size_t blockCount = renderBlockCount(controls.decayMs);
  for (size_t block = 1; block < blockCount; ++block) {
    if (SynthEngine::processAudioBlock(controls).hitDetected) {
      unexpectedRetrigger = true;
    }
  }

  return {
      hitDetected,
      unexpectedRetrigger,
      FakeAudioIo::writtenSamples(),
  };
}

VelocityRenderResult renderRisingPadHit(uint16_t peak,
                                        const SynthControls& controls) {
  resetHitDetector(controls.sensitivity);
  FakeAudioIo::reset();
  FakeAudioIo::simulateRisingPadImpulse(peak);

  const SynthEngine::ProcessResult result =
      SynthEngine::processAudioBlock(controls);
  TEST_ASSERT_TRUE_MESSAGE(result.hitDetected,
                           "The rising pad impulse did not trigger a hit");
  return {result.velocity, FakeAudioIo::writtenSamples()};
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
  TEST_ASSERT_EQUAL_UINT32(
      renderBlockCount(TEST_CONTROLS.decayMs) * AudioIo::BLOCK_FRAMES,
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
  constexpr uint16_t WEAK_PEAK =
      AppConfig::HitDetection::PAD_INPUT_MIN +
      (AppConfig::HitDetection::PAD_INPUT_MAX -
       AppConfig::HitDetection::PAD_INPUT_MIN) /
          10;
  verifyAudioPath(
      {"weak", "Audio path - weak pad hit", WEAK_PEAK, 700, 900});
}

void testMediumPadHit() {
  constexpr uint16_t MEDIUM_PEAK =
      AppConfig::HitDetection::PAD_INPUT_MIN +
      (AppConfig::HitDetection::PAD_INPUT_MAX -
       AppConfig::HitDetection::PAD_INPUT_MIN) /
          2;
  verifyAudioPath({"medium", "Audio path - medium pad hit", MEDIUM_PEAK,
                   6900, 7400});
}

void testMaximumPadHit() {
  verifyAudioPath({"maximum", "Audio path - maximum pad hit",
                   AppConfig::HitDetection::PAD_INPUT_MAX, 15500, 16500});
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
  TEST_ASSERT_EQUAL_UINT16(1, result.activeSamples);
  TEST_ASSERT_EQUAL_UINT16(0, result.tailMaximum);
  TEST_ASSERT_EQUAL_UINT16(0, result.tailActiveSamples);
  TEST_ASSERT_EQUAL_UINT32(3000, result.windowEnergy);
}

void testVoltageJumpWithWeakTailIsRejected() {
  resetHitDetector();
  std::vector<uint16_t> magnitudes(AudioIo::BLOCK_FRAMES, 0);
  constexpr size_t CANDIDATE_SAMPLE = 20;
  magnitudes[CANDIDATE_SAMPLE] = 1000;
  for (size_t sample = CANDIDATE_SAMPLE + 1;
       sample < CANDIDATE_SAMPLE +
                    AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES;
       ++sample) {
    magnitudes[sample] = 150;
  }

  const HitDetector::Result result = HitDetector::process(
      magnitudes.data(), magnitudes.size(), TEST_CONTROLS.sensitivity);

  TEST_ASSERT_TRUE(result.candidateStarted);
  TEST_ASSERT_TRUE(result.validationCompleted);
  TEST_ASSERT_FALSE(result.hitDetected);
  TEST_ASSERT_EQUAL_UINT16(1000, result.validationMax);
  TEST_ASSERT_EQUAL_UINT16(
      AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES,
      result.activeSamples);
  TEST_ASSERT_EQUAL_UINT16(150, result.tailMaximum);
  TEST_ASSERT_EQUAL_UINT16(
      AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES -
          AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE,
      result.tailActiveSamples);
}

void testSustainedLightPadTailIsAccepted() {
  resetHitDetector();
  std::vector<uint16_t> magnitudes(AudioIo::BLOCK_FRAMES, 0);
  constexpr size_t CANDIDATE_SAMPLE = 20;
  magnitudes[CANDIDATE_SAMPLE] = 180;
  magnitudes[CANDIDATE_SAMPLE + 1] = 420;
  for (size_t sample = CANDIDATE_SAMPLE + 2;
       sample < CANDIDATE_SAMPLE +
                    AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE;
       ++sample) {
    magnitudes[sample] = 160;
  }
  for (size_t sample = CANDIDATE_SAMPLE +
                       AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE;
       sample < CANDIDATE_SAMPLE +
                    AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES;
       ++sample) {
    magnitudes[sample] = 110;
  }

  const HitDetector::Result result = HitDetector::process(
      magnitudes.data(), magnitudes.size(), TEST_CONTROLS.sensitivity);

  TEST_ASSERT_TRUE(result.candidateStarted);
  TEST_ASSERT_TRUE(result.validationCompleted);
  TEST_ASSERT_TRUE(result.hitDetected);
  TEST_ASSERT_EQUAL_UINT16(420, result.validationMax);
  TEST_ASSERT_EQUAL_UINT16(
      AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES,
      result.activeSamples);
  TEST_ASSERT_EQUAL_UINT16(110, result.tailMaximum);
  TEST_ASSERT_EQUAL_UINT16(
      AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES -
          AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE,
      result.tailActiveSamples);
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
  for (size_t sample = 0;
       sample < AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE - 1;
       ++sample) {
    secondBlock[sample] = 200;
  }
  for (size_t sample =
           AppConfig::HitDetection::TRIGGER_TAIL_START_SAMPLE - 1;
       sample < AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES - 1;
       ++sample) {
    secondBlock[sample] = 110;
  }
  const HitDetector::Result secondResult = HitDetector::process(
      secondBlock.data(), secondBlock.size(), TEST_CONTROLS.sensitivity);

  TEST_ASSERT_TRUE(secondResult.validationCompleted);
  TEST_ASSERT_TRUE(secondResult.hitDetected);
  TEST_ASSERT_EQUAL_UINT16(420, secondResult.validationMax);
  TEST_ASSERT_EQUAL_UINT16(
      AppConfig::HitDetection::TRIGGER_VALIDATION_SAMPLES,
      secondResult.activeSamples);
  TEST_ASSERT_EQUAL_UINT16(110, secondResult.tailMaximum);
}

void testAcceptedHitStaysLockedOutUntilQuietBlock() {
  resetHitDetector();
  FakeAudioIo::reset();
  FakeAudioIo::simulatePadImpulse(600);
  TEST_ASSERT_TRUE(
      SynthEngine::processAudioBlock(TEST_CONTROLS).hitDetected);

  FakeAudioIo::simulatePadImpulse(500);
  TEST_ASSERT_FALSE(
      SynthEngine::processAudioBlock(TEST_CONTROLS).hitDetected);
  TEST_ASSERT_FALSE(
      SynthEngine::processAudioBlock(TEST_CONTROLS).hitDetected);

  FakeAudioIo::simulatePadImpulse(500);
  TEST_ASSERT_TRUE(
      SynthEngine::processAudioBlock(TEST_CONTROLS).hitDetected);
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

  SynthVoice::trigger(1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 300.0f,
                      0.0f);
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
  SynthVoice::trigger(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 300.0f,
                      0.0f);
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

  SynthVoice::trigger(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 300.0f,
                      0.0f);
  TEST_ASSERT_TRUE(blockContainsNonZeroSample(SynthVoice::render()));
}

void testAmpVelocityInterpolatesBetweenConstantAndFullDynamics() {
  constexpr float VELOCITY = 0.2f;
  constexpr float QUARTER_SAMPLE_RATE = AudioIo::SAMPLE_RATE / 4.0f;

  SynthVoice::trigger(VELOCITY, QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 0.0f,
                      0.0f, 300.0f, 0.0f);
  const int32_t constantAmplitude = SynthVoice::render()[1];

  SynthVoice::trigger(VELOCITY, QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 1.0f,
                      0.0f, 300.0f, 0.0f);
  const int32_t velocityAmplitude = SynthVoice::render()[1];

  TEST_ASSERT_TRUE(constantAmplitude > velocityAmplitude * 2.4f);
  TEST_ASSERT_TRUE(constantAmplitude < velocityAmplitude * 2.6f);
}

void testStrongHitIsLouderWithFullAmpVelocity() {
  constexpr float QUARTER_SAMPLE_RATE = AudioIo::SAMPLE_RATE / 4.0f;

  SynthVoice::trigger(AppConfig::Voice::AMP_VELOCITY_FULL_SCALE,
                      QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 0.0f, 0.0f, 300.0f,
                      0.0f);
  const int32_t constantAmplitude = SynthVoice::render()[1];

  SynthVoice::trigger(AppConfig::Voice::AMP_VELOCITY_FULL_SCALE,
                      QUARTER_SAMPLE_RATE, 0.0f, 0.0f, 1.0f, 0.0f, 300.0f,
                      0.0f);
  const int32_t fullVelocityAmplitude = SynthVoice::render()[1];

  TEST_ASSERT_TRUE(fullVelocityAmplitude > constantAmplitude * 1.8f);
  TEST_ASSERT_TRUE(fullVelocityAmplitude < constantAmplitude * 1.85f);
}

void testShapeMorphEndpointsAndPulseWidth() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, Waveforms::triangle(0.1f),
                           Waveforms::shapedPulse(0.1f, 0.0f, 0.08f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f,
                           Waveforms::shapedPulse(0.1f, 0.5f, 0.08f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.0f,
                           Waveforms::shapedPulse(0.6f, 0.5f, 0.08f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f,
                           Waveforms::shapedPulse(0.07f, 1.0f, 0.08f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.0f,
                           Waveforms::shapedPulse(0.09f, 1.0f, 0.08f));
}

size_t firstNegativeSample(const int32_t* samples) {
  for (size_t sample = 0; sample < AudioIo::BLOCK_FRAMES; ++sample) {
    if (samples[sample] < 0) return sample;
  }
  return AudioIo::BLOCK_FRAMES;
}

size_t firstNegativeOutputSample(const std::vector<int16_t>& samples) {
  for (size_t sample = 0; sample < samples.size(); ++sample) {
    if (samples[sample] < 0) return sample;
  }
  return samples.size();
}

void testPitchVelocityRespondsStronglyToHitStrength() {
  constexpr float BASE_FREQUENCY_HZ = 200.0f;
  SynthVoice::trigger(0.0f, BASE_FREQUENCY_HZ, 0.0f, 0.0f, 0.0f, 0.5f,
                      300.0f,
                      AppConfig::Controls::ENV_TO_PITCH_MAX_SEMITONES);
  const size_t weakHitTransition = firstNegativeSample(SynthVoice::render());

  SynthVoice::trigger(1.0f, BASE_FREQUENCY_HZ, 0.0f, 0.0f, 0.0f, 0.5f,
                      300.0f,
                      AppConfig::Controls::ENV_TO_PITCH_MAX_SEMITONES);
  const size_t strongHitTransition =
      firstNegativeSample(SynthVoice::render());

  TEST_ASSERT_TRUE(weakHitTransition > 100);
  TEST_ASSERT_TRUE(strongHitTransition < 20);
}

void testMaximumClickHasClearTransientLevel() {
  SynthVoice::trigger(1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 300.0f,
                      0.0f);
  const int32_t* samples = SynthVoice::render();
  int32_t peak = 0;
  for (size_t sample = 0; sample < AudioIo::BLOCK_FRAMES; ++sample) {
    const int32_t magnitude = samples[sample] < 0 ? -samples[sample]
                                                  : samples[sample];
    if (magnitude > peak) peak = magnitude;
  }
  TEST_ASSERT_TRUE(peak > 18000);
}

void testAmpVelocityUsesFullRisingImpulsePeak() {
  SynthControls controls = TEST_CONTROLS;
  controls.sensitivity = 0.0f;
  controls.pitchDropOctaves = 0.0f;
  controls.clickLevel = 0.0f;
  controls.ampVelocity = 1.0f;
  controls.shapeNormalized = 0.5f;
  controls.envToPitchSemitones = 0.0f;

  const VelocityRenderResult weak =
      renderRisingPadHit(WEAK_RISING_PEAK, controls);
  const VelocityRenderResult strong =
      renderRisingPadHit(STRONG_RISING_PEAK, controls);
  const int16_t weakPeak = findOutputPeak(weak.outputSamples);
  const int16_t strongPeak = findOutputPeak(strong.outputSamples);

  TEST_ASSERT_TRUE(strong.velocity > weak.velocity + 0.5f);
  TEST_ASSERT_TRUE(strongPeak > weakPeak * 20);
}

void testPitchVelocityUsesFullRisingImpulsePeak() {
  SynthControls controls = TEST_CONTROLS;
  controls.sensitivity = 1.0f;
  controls.pitchDropOctaves = 0.0f;
  controls.clickLevel = 0.0f;
  controls.ampVelocity = 0.0f;
  controls.shapeNormalized = 0.5f;
  controls.envToPitchSemitones =
      AppConfig::Controls::ENV_TO_PITCH_MAX_SEMITONES;

  const VelocityRenderResult weak =
      renderRisingPadHit(WEAK_RISING_PEAK, controls);
  const VelocityRenderResult strong =
      renderRisingPadHit(STRONG_RISING_PEAK, controls);
  const size_t weakTransition =
      firstNegativeOutputSample(weak.outputSamples);
  const size_t strongTransition =
      firstNegativeOutputSample(strong.outputSamples);

  TEST_ASSERT_TRUE(strong.velocity > weak.velocity + 0.7f);
  TEST_ASSERT_TRUE(weakTransition > strongTransition * 4);
}

void testSensitivityCurvesAndExportArtifacts() {
  constexpr float MINIMUM_SENSITIVITY = 0.0f;
  constexpr float MAXIMUM_SENSITIVITY = 1.0f;
  const uint16_t middlePeak = AppConfig::HitDetection::PAD_INPUT_MAX / 2;
  const float lowSensitivityVelocity =
      HitDetector::mapVelocity(middlePeak, MINIMUM_SENSITIVITY);
  const float highSensitivityVelocity =
      HitDetector::mapVelocity(middlePeak, MAXIMUM_SENSITIVITY);

  TEST_ASSERT_FLOAT_WITHIN(
      0.0001f, 0.0f,
      HitDetector::mapVelocity(AppConfig::HitDetection::PAD_INPUT_MIN,
                               MINIMUM_SENSITIVITY));
  TEST_ASSERT_TRUE(highSensitivityVelocity > lowSensitivityVelocity * 2.5f);
  TEST_ASSERT_FLOAT_WITHIN(
      0.0001f, 1.0f,
      HitDetector::mapVelocity(
          static_cast<uint16_t>(
              AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MIN_SENSITIVITY),
          MINIMUM_SENSITIVITY));
  TEST_ASSERT_FLOAT_WITHIN(
      0.0001f, 1.0f,
      HitDetector::mapVelocity(
          static_cast<uint16_t>(
              AppConfig::HitDetection::VELOCITY_EFFECTIVE_MAX_AT_MAX_SENSITIVITY),
          MAXIMUM_SENSITIVITY));

  TEST_ASSERT_TRUE_MESSAGE(
      SensitivitySvg::write(artifactDirectory() / "sensitivity_minimum.svg",
                            "Sensitivity response - minimum",
                            MINIMUM_SENSITIVITY),
      "Could not write the minimum sensitivity SVG artifact");
  TEST_ASSERT_TRUE_MESSAGE(
      SensitivitySvg::write(artifactDirectory() / "sensitivity_maximum.svg",
                            "Sensitivity response - maximum",
                            MAXIMUM_SENSITIVITY),
      "Could not write the maximum sensitivity SVG artifact");
}

void testDecayControlsMainVoiceDuration() {
  const size_t minimumDecayBlocks =
      renderBlockCount(AppConfig::Controls::DECAY_MIN_MS);

  SynthVoice::trigger(1.0f, 200.0f, 0.0f, 0.0f, 1.0f, 0.5f,
                      AppConfig::Controls::DECAY_MIN_MS, 0.0f);
  for (size_t block = 0; block < minimumDecayBlocks; ++block) {
    SynthVoice::render();
  }
  TEST_ASSERT_FALSE(blockContainsNonZeroSample(SynthVoice::render()));

  SynthVoice::trigger(1.0f, 200.0f, 0.0f, 0.0f, 1.0f, 0.5f,
                      AppConfig::Controls::DECAY_MAX_MS, 0.0f);
  for (size_t block = 0; block < minimumDecayBlocks; ++block) {
    SynthVoice::render();
  }
  TEST_ASSERT_TRUE(blockContainsNonZeroSample(SynthVoice::render()));
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testSingleSampleElectricalSpikeIsRejected);
  RUN_TEST(testVoltageJumpWithWeakTailIsRejected);
  RUN_TEST(testSustainedLightPadTailIsAccepted);
  RUN_TEST(testValidationContinuesAcrossInputBlocks);
  RUN_TEST(testAcceptedHitStaysLockedOutUntilQuietBlock);
  RUN_TEST(testClickIsSilentAtMinimumLevel);
  RUN_TEST(testClickDecaysAndCanBeRetriggered);
  RUN_TEST(testAmpVelocityInterpolatesBetweenConstantAndFullDynamics);
  RUN_TEST(testStrongHitIsLouderWithFullAmpVelocity);
  RUN_TEST(testShapeMorphEndpointsAndPulseWidth);
  RUN_TEST(testPitchVelocityRespondsStronglyToHitStrength);
  RUN_TEST(testMaximumClickHasClearTransientLevel);
  RUN_TEST(testAmpVelocityUsesFullRisingImpulsePeak);
  RUN_TEST(testPitchVelocityUsesFullRisingImpulsePeak);
  RUN_TEST(testSensitivityCurvesAndExportArtifacts);
  RUN_TEST(testDecayControlsMainVoiceDuration);
  RUN_TEST(testWeakPadHit);
  RUN_TEST(testMediumPadHit);
  RUN_TEST(testMaximumPadHit);
  RUN_TEST(testMixedClickAndVoiceArtifact);
  return UNITY_END();
}
