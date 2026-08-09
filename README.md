# Synth Tom ESP32

Synth Tom ESP32 is an electronic drum synth build on the ESP32 Audio Kit V2.2 with the ESP32-A1S module and ES8388 audio codec.

The firmware currently:

- reads drum pad impulses from the right channel of ES8388 LINE IN,
- validates each trigger candidate from the shape of an eight-sample impulse window and rejects isolated electrical spikes,
- maps accepted peaks into normalized velocity using a sensitivity control,
- triggers one monophonic synth-drum voice per hit,
- morphs the main oscillator continuously from triangle through square to a narrow pulse, applies shared exponential amplitude and pitch envelopes, and mixes in a short independent noise click at the start of each accepted hit,
- applies output headroom and final brickwall limiting,
- sends the synthesized mono signal to the left audio output,
- streams the measured input peak and trigger-validation diagnostics to Teleplot,
- reads potentiometers from an ADS7830 about every 5 ms,
- shows accepted hit velocity on a PWM sensitivity LED with a visible decay.

## Code structure

The firmware is organized from general application flow to hardware and signal processing details:

- `main.cpp` enters `Application::begin()` and `Application::update()`.
- `application.cpp` coordinates the audio path and user interface, explicitly passing the current UI control state to the synth engine.
- `synth_engine.cpp` shows the complete synthesis path: read the pad input, detect and trigger a hit, render the voice, and write the output block.
- `hit_detector.cpp` owns the trigger-validation state, hit re-arming, and peak to velocity mapping using the current sensitivity.
- `synth_voice.cpp` defines the sound of one voice. Its public `trigger()` and `render()` functions delegate envelope, pitch, phase, and mono-buffer work to focused private helpers.
- `synth_controls.h` defines the hardware-independent runtime control values.
- `synth_control_input.cpp` scans and filters the potentiometers, maps their positions to runtime controls, and contains control-input diagnostics.
- `output_limiter.cpp` applies the final master gain and a block-lookahead sample-peak limiter before samples are sent to I2S.
- `waveforms.cpp` provides the reusable waveform generator used by the synth voice.
- `math_utils.cpp` owns small reusable numeric helpers shared across domains.
- `user_interface.cpp` owns the UI lifecycle and exposes current synth controls; it delegates potentiometer details to `synth_control_input.cpp` and maps accepted-hit velocity to sensitivity LED brightness and decay.
- `audio_io.cpp`, `ads7830.cpp`, and `es8388.cpp` contain hardware-level details.

## Pad input wiring

The drum pad is connected to the ESP32 LINE IN through the following input protection and filtering circuit:

```text
PAD TIP
   o
   |
 [100k]
   |
   +---------- SIGNAL ----------||----------> ESP32 LINE IN
   |                            100nF
   |
 [10k]
   |
  GND

SIGNAL ----||---- GND
           1nF

SIGNAL ----|>|----+
                  |
SIGNAL ----|<|----+---- GND
       Schottky clamps

PAD SLEEVE ---------------------------- GND
```

## User interface

### Potentiometers

The ADS7830 uses a separate I2C bus from the ES8388 codec:

- SDA: GPIO23
- SCL: GPIO18
- I2C address: `0x48`

The tested board is labeled `ADS7830 STEMMA QT`. Its working reference wiring is:

- REF: connected to 3.3V,
- COM: left externally unconnected,
- `Ext Ref` solder jumper: closed,
- `Ext Com` solder jumper: closed.

With the `Ext Com` jumper closed, the working setup uses the board's COM connection and leaves the external COM pin unconnected. All 10kΩ potentiometers are wired in the same way, with a 680Ω series resistor and a 100nF capacitor filtering each wiper before its individual ADS7830 channel:

```text
POT left leg  ---- GND
POT right leg ---- 3.3V

POT wiper ----[680R]----+------> ADS7830 CHx
                        |
                       === 100nF
                        |
                       GND
```

`CHx` is a different ADS7830 input for each potentiometer. The GND and 3.3V rails are daisy-chained from one potentiometer to the next across the complete set. The channels and their mappings are:

- A0 — SENSITIVITY: controls the pad peak-to-velocity response from harder at minimum to more responsive at maximum by changing both the response curve and the input peak that maps to maximum velocity.
- A1 — PITCH: selects the base oscillator frequency from 45 Hz to 1200 Hz using an exponential mapping.
- A2 — PITCH DROP: selects a pitch-envelope depth from 0 to 4.5 octaves using a squared mapping for finer adjustment near zero. The depth is scaled from 65% for a zero-velocity hit to 100% for a maximum-velocity hit.
- A3 — CLICK: controls the level of the fixed 8 ms noise transient from off to its configured maximum. Hit velocity separately scales the click from 50% to 100% of the selected level.
- A4 — AMP VEL: controls the influence of hit velocity on main-oscillator amplitude, from a constant 55% gain at minimum to full velocity dynamics at maximum. In the full-dynamics position, the oscillator reaches full gain at velocity 0.9.
- A5 — SHAPE: morphs the main oscillator from triangle at minimum through square at the midpoint to an 8% pulse at maximum.
- A6 — DECAY: selects the exponential main-envelope duration from 30 ms to 2400 ms. The selected duration is used by both amplitude and pitch envelopes.
- A7 — PITCH VEL: selects an additional velocity-sensitive pitch-envelope depth from 0 to 36 semitones. The contribution used for a hit is the selected depth multiplied by that hit's normalized velocity, so at the maximum setting it ranges from 0 semitones for velocity 0 to 36 semitones (three octaves) for velocity 1. It is added to the A2 PITCH DROP depth: the oscillator starts above the A1 base pitch by their combined amount and falls back toward the base pitch with the A6 decay, subject to the 8 kHz frequency cap.

All channels are sampled every 5 ms. An EMA filter and a two-count dead zone stabilize the native 8-bit readings. Every control maps a higher ADC reading to a higher control value. Trigger acceptance is determined by the short eight-sample impulse-shape window, but velocity is mapped from the full peak captured across the audio block so a rising impulse is not reduced to its early threshold-crossing level. A0 is used when mapping each accepted hit, and A5 is applied at control rate so it can change the waveform while a voice is sounding. The sound parameters from A1, A2, A3, A4, A6, and A7 are captured when a hit triggers and do not modify an already sounding voice. A3 controls only the click, which retains its own fixed 8 ms decay, while A4 controls only the main oscillator amplitude and does not alter either envelope duration.

### Sensitivity LED

The sensitivity LED is connected from GPIO22 through a resistor to its anode, with its cathode connected to GND. It is active-high and uses 5 kHz, 8-bit hardware PWM updated every 5 ms.

Only an accepted drum-pad hit updates the LED. The LED receives the same final `velocity` value used to trigger the synth voice after sensitivity mapping and clamping. Brightness uses `sqrt(velocity)`, making light hits easier to see, then decays exponentially to off in about 150 ms. A new accepted hit immediately replaces the current level instead of accumulating it. Pin, PWM, and fade values are configured in the `AppConfig::Ui` section of `include/app_config.h`.

## Logging

Serial output categories can be enabled independently in the `AppConfig::Diagnostics` and `AppConfig::Controls` sections of `include/app_config.h`:

- `AppConfig::Diagnostics::LOG_AUDIO_PEAKS` — streams ES8388 input peaks in Teleplot format as `rawPeak`,
- `AppConfig::Diagnostics::LOG_TRIGGER_VALIDATION` — reports `triggerCandidate`, `triggerAccepted`, `validationMax`, final mapped `velocity`, `activeSamples`, and `windowEnergy` when a candidate starts or a validation finishes,
- `AppConfig::Controls::LOG_CONTROL_VALUES` — prints throttled filtered and mapped potentiometer values,
- `AppConfig::Diagnostics::LOG_FATAL_ERRORS` — reports a fatal hardware-initialization failure before stopping the application.

All logging categories are disabled by default, and the serial port is not initialized while they remain disabled. A fatal initialization failure therefore stops the application silently in the default configuration. When enabled, validation details are event-driven rather than sent for every audio block, which limits their timing impact. Control logging prints all eight filtered ADC values plus the mapped control values, including `shape`, `decay_ms`, and the maximum `env_to_pitch` depth in semitones.

For tuning, an isolated electrical spike should normally show `activeSamples:1` and `triggerAccepted:0`. A pad hit should retain all eight samples at or above the follow threshold and show `triggerAccepted:1`. Compare `rawPeak` with the resulting `velocity` for light, medium, and strong hits when calibrating `PAD_INPUT_MIN`, `PAD_INPUT_MAX`, and SENSITIVITY. `validationMax` and `windowEnergy` describe only the short acceptance window and do not determine hit strength after acceptance. `windowEnergy` is diagnostic; it is not part of the acceptance condition.

## Sound parameters

Envelope, amplitude, and pitch-envelope behavior are configured in the `AppConfig::Voice` section of `include/app_config.h`. Potentiometer scanning, physical control ranges, channel assignment, and initial control values are kept in `AppConfig::Controls` for hardware tuning. Pad triggering and sensitivity response curves are configured in `AppConfig::HitDetection`.

The final output stage is configured in `AppConfig::AudioOutput`. It applies a master gain of `0.5` (about -6 dB), then protects the codec input with a brickwall ceiling at `0.8` of full scale (about -1.9 dBFS). The limiter uses immediate block attack and a configurable release; it preserves the velocity dynamics instead of simply clipping the synthesized waveform.

- `MIN_PULSE_WIDTH` — narrowest pulse produced at maximum SHAPE, set to `0.08`.
- `ENVELOPE_SILENCE_THRESHOLD` — level reached by the shared exponential main envelope at the end of the selected DECAY duration.
- `CLICK_DECAY_MS` — fixed exponential click duration, set to about 8 ms.
- `CLICK_MAX_AMPLITUDE` — maximum click scale relative to the oscillator, set to `0.65` for a clearly audible transient.
- `AMP_VELOCITY_FULL_SCALE` — velocity at which full AMP VEL dynamics reach maximum oscillator level; defaults to `0.9`.
- `AMP_VELOCITY_CONSTANT_GAIN` — oscillator gain at minimum AMP VEL, set to `0.55` so increasing the control can boost hard hits as well as attenuate soft hits.
- `PITCH_DROP_VELOCITY_MIN_SCALE` — minimum share of the selected pitch drop applied to low-velocity hits.
- `MAX_START_FREQUENCY_HZ` — upper limit for the initial pitch-envelope frequency.

At each trigger, the existing A2 pitch-drop depth retains its 65–100% velocity scaling and is combined with A7 PITCH VEL. The A7 contribution ranges from zero for the weakest hit to the full selected depth for maximum velocity, making hit strength clearly affect the pitch sweep. Both pitch contributions use the same exponential envelope selected by A6. The pitch-envelope frequency is capped at 8 kHz, while A1 selects the base pitch across its full configured range. The exponential decay range and maximum A7 depth are configured in `AppConfig::Controls` as `DECAY_MIN_MS`, `DECAY_MAX_MS`, and `ENV_TO_PITCH_MAX_SEMITONES`.

The I2S stream remains active continuously and idle output is exact-zero PCM. ES8388 DAC Control 3 is restored to `0x22` after codec initialization, which leaves the DAC unmuted while preserving its default soft-ramp and control bits. Clearing the complete register to `0x00` caused a repeatable output pop after the codec received sustained digital silence.

Input-related defaults are stored in the `AppConfig::AudioInput` and `AppConfig::HitDetection` sections of the same file:

- `AppConfig::AudioInput::CHANNEL` — selected LINE IN channel.
- `AppConfig::AudioInput::GAIN_CODE` — ES8388 input PGA gain.
- `TRIGGER_PRE_THRESHOLD = 180` — starts validation when a magnitude reaches this level.
- `TRIGGER_VALIDATION_SAMPLES = 8` — window length, including the candidate sample (about 0.18 ms at 44.1 kHz).
- `TRIGGER_MIN_ACTIVE_SAMPLES = 8` — minimum number of samples needed to reject the single-spike shape.
- `TRIGGER_FOLLOW_THRESHOLD = 100` — magnitude at which a window sample counts as active.
- `TRIGGER_REARM_THRESHOLD = 120` — a complete input block must stay below this value before another accepted hit can trigger the voice.
- `PAD_INPUT_MIN` and `PAD_INPUT_MAX` — programmed calibration points for velocity mapping.
- `VELOCITY_EFFECTIVE_MAX_*` and `VELOCITY_CURVE_*` — endpoints of the sensitivity-dependent velocity response.

## Run

The default PlatformIO environment builds the ESP32 firmware. The native environment runs the audio signal-path tests described below.

Build and upload the firmware:

```sh
pio run -t upload
```

Serial output is disabled by default. To use it for diagnostics, enable the required logging category in `include/app_config.h`, select the board serial port, and use `115200` baud. `LOG_AUDIO_PEAKS` provides Teleplot peak monitoring, `LOG_TRIGGER_VALIDATION` helps tune the trigger filter, `LOG_CONTROL_VALUES` reports the ADC controls, and `LOG_FATAL_ERRORS` reports hardware-initialization failures.

## Audio signal-path tests

The native integration test exercises the complete hardware-independent audio path: a simulated pad impulse enters through the `AudioIo` boundary, then the real hit detector, synth voice, and output limiter generate the captured mono output. It verifies rejection of a single-sample spike, acceptance of a light pad tail, validation across an input-block boundary, and lockout behavior. Weak, medium, and maximum scenarios derive their peaks from the configured `PAD_INPUT_MIN` and `PAD_INPUT_MAX`, so they remain representative after calibration changes. Rising-impulse regression cases keep the eight validation samples identical and place the actual peak later in the block; they verify that the final peak produces large differences in both AMP VEL output level and PITCH VEL frequency. Focused voice tests also verify click silence and transient level, decay/retrigger behavior, both ends of the AMP VEL interpolation, hard-hit gain at maximum AMP VEL, SHAPE endpoints and pulse width, velocity-sensitive A7 pitch modulation, and the A6 main-voice duration range.

Run the tests from the project root:

```sh
pio test -e native
```

Each executed scenario writes a self-contained SVG waveform plot to `test/artifacts/`. Audio-path filenames contain both the hit strength and configured simulated input peak. The plots include the input peak, synthesis parameters (including CLICK and AMP VEL), output peak, normalized amplitude, and time axis. The click decay test additionally writes `audio_path_click_envelope_level_1.svg`, a 12 ms close-up of the isolated click at maximum level. The mixed-signal test writes a 20 ms close-up of the click mixed with the regular 150 Hz voice before output. The sensitivity test writes `sensitivity_minimum.svg` and `sensitivity_maximum.svg`, plotting captured pad peak against mapped velocity at both ends of the SENSITIVITY control. All plots are regenerated and overwritten on every test run.
