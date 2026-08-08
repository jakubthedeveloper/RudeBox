# Synth Tom ESP32

Synth Tom ESP32 is an electronic drum synth prototype for the ESP32 Audio Kit
V2.2 with the ESP32-A1S module and ES8388 audio codec.

The firmware currently:

- reads drum pad impulses from the right channel of ES8388 LINE IN,
- validates each trigger candidate from the shape of an eight-sample impulse
  window and rejects isolated electrical spikes,
- maps accepted peaks into normalized velocity using a sensitivity control,
- triggers one monophonic synth-drum voice per hit,
- generates a triangle waveform with amplitude and pitch envelopes and mixes
  in a short noise click at the start of each accepted hit,
- applies output headroom and final brickwall limiting,
- sends the synthesized mono signal to the left audio output,
- streams the measured input peak and trigger-validation diagnostics to
  Teleplot,
- reads five potentiometers from an ADS7830 about every 5 ms,
- shows accepted hit velocity on a PWM sensitivity LED with a visible decay.

## Code structure

The firmware is organized from general application flow to hardware and signal
processing details:

- `main.cpp` enters `Application::begin()` and `Application::update()`.
- `application.cpp` coordinates the audio path and user interface, explicitly
  passing the current UI control state to the synth engine.
- `synth_engine.cpp` shows the complete synthesis path: read the pad input,
  detect and trigger a hit, render the voice, and write the output block.
- `hit_detector.cpp` owns the trigger-validation state, hit re-arming, and peak
  to velocity mapping using the current sensitivity.
- `synth_voice.cpp` defines the sound of one voice. Its public `trigger()` and
  `render()` functions delegate envelope, pitch, phase, and mono-buffer work
  to focused private helpers.
- `synth_controls.h` defines the hardware-independent runtime control values.
- `synth_control_input.cpp` scans and filters the potentiometers, maps their
  positions to runtime controls, and contains control-input diagnostics.
- `output_limiter.cpp` applies the final master gain and a block-lookahead
  sample-peak limiter before samples are sent to I2S.
- `waveforms.cpp` provides the reusable waveform generator used by the synth
  voice.
- `math_utils.cpp` owns small reusable numeric helpers shared across domains.
- `user_interface.cpp` owns the UI lifecycle and exposes current synth controls;
  it delegates potentiometer details to `synth_control_input.cpp` and maps
  accepted-hit velocity to sensitivity LED brightness and decay.
- `audio_io.cpp`, `ads7830.cpp`, and `es8388.cpp` contain hardware-level details.

## User interface

### Potentiometers

The ADS7830 uses a separate I2C bus from the ES8388 codec:

- SDA: GPIO23
- SCL: GPIO18
- I2C address: `0x48`

The tested board is labeled `ADS7830 STEMMA QT`. Its working reference wiring
is:

- REF: connected to 3.3V,
- COM: left externally unconnected,
- `Ext Ref` solder jumper: closed,
- `Ext Com` solder jumper: closed.

With the `Ext Com` jumper closed, the working setup uses the board's COM
connection and leaves the external COM pin unconnected. Each 10kΩ potentiometer
is used as a voltage divider between 3.3V and GND, with its wiper connected
through a 1kΩ series resistor to its ADS7830 input:

- A0 — sensitivity of the pad peak-to-velocity response,
- A1 — base oscillator pitch from 45 Hz to 1200 Hz,
- A2 — pitch-drop depth from 0 to 4.5 octaves,
- A3 — level of the short attack click from off to maximum,
- A4 — influence of hit velocity on oscillator amplitude, from constant
  amplitude to full velocity dynamics.

All channels are sampled every 5 ms. An EMA filter and a two-count dead zone
stabilize the native 8-bit readings. Pitch uses exponential mapping, while
pitch-drop uses a squared curve for finer control near zero. Trigger acceptance
is determined by the short impulse-shape window, and the sensitivity control
maps the accepted window maximum to velocity. Every control maps a higher ADC
reading to a higher control value. A3 controls only the click level; the click
retains a fixed mild velocity response. A4 controls only the main oscillator
and does not alter either envelope duration.

### Sensitivity LED

The sensitivity LED is connected from GPIO22 through a resistor to its
anode, with its cathode connected to GND. It is active-high and uses 5 kHz,
8-bit hardware PWM updated every 5 ms.

Only an accepted drum-pad hit updates the LED. The LED receives the same final
`velocity` value used to trigger the synth voice after sensitivity mapping and
clamping. Brightness uses `sqrt(velocity)`, making light hits easier to see,
then decays exponentially to off in about 150 ms. A new accepted hit immediately
replaces the current level instead of accumulating it. Pin, PWM, and fade values
are configured in the `AppConfig::Ui` section of `include/app_config.h`.

## Logging

Serial output categories can be enabled independently in the
`AppConfig::Diagnostics` and `AppConfig::Controls` sections of
`include/app_config.h`:

- `AppConfig::Diagnostics::LOG_AUDIO_PEAKS` — streams ES8388 input peaks in
  Teleplot format as `rawPeak`,
- `AppConfig::Diagnostics::LOG_TRIGGER_VALIDATION` — reports
  `triggerCandidate`, `triggerAccepted`, `validationMax`, `activeSamples`, and
  `windowEnergy` when a candidate starts or a validation finishes,
- `AppConfig::Controls::LOG_CONTROL_VALUES` — prints throttled filtered and
  mapped potentiometer values.

Audio-peak and trigger-validation diagnostics are disabled by default.
Validation details are event-driven rather than sent for every audio block,
which limits their timing impact. Control logging is currently enabled and
prints all five filtered ADC values plus normalized `CLICK` and `AMP_VEL`
values. Fatal initialization errors are always printed.

For tuning, an isolated electrical spike should normally show
`activeSamples:1` and `triggerAccepted:0`. A pad hit should retain all eight
samples at or above the follow threshold and show `triggerAccepted:1`. Compare
`validationMax` and `windowEnergy` for switch-on noise, switch-off noise, and
light, medium, and strong hits before adjusting the thresholds. `windowEnergy`
is diagnostic in this initial implementation; it is not part of the acceptance
condition.

## Sound parameters

Envelope, amplitude, and pitch-envelope behavior are configured in the
`AppConfig::Voice` section of `include/app_config.h`. Potentiometer scanning,
physical control ranges, channel assignment, and initial control values are
kept in `AppConfig::Controls` for hardware tuning. Pad triggering and
sensitivity response curves are configured in `AppConfig::HitDetection`.

The final output stage is configured in `AppConfig::AudioOutput`. It applies a
master gain of `0.5` (about -6 dB), then protects the codec input with a
brickwall ceiling at `0.8` of full scale (about -1.9 dBFS). The limiter uses
immediate block attack and a configurable release; it preserves the velocity
dynamics instead of simply clipping the synthesized waveform.

- `AMP_RELEASE_MS` — duration of the amplitude envelope and therefore the sound.
- `PITCH_DECAY_MS` — time taken by the pitch envelope to fall to the base
  frequency.
- `CLICK_DECAY_MS` — fixed exponential click duration, set to about 8 ms.
- `CLICK_MAX_AMPLITUDE` — maximum click scale relative to the oscillator.
- `AMP_VELOCITY_FULL_SCALE` — velocity above which the oscillator reaches the
  same full level as with AMP VEL at minimum; defaults to `0.9`.
- `PITCH_DROP_VELOCITY_MIN_SCALE` — minimum share of the selected pitch drop
  applied to low-velocity hits.
- `MAX_START_FREQUENCY_HZ` — upper limit for the initial pitch-envelope
  frequency.

At each trigger, the A2 depth is mildly scaled from 65% to 100% by velocity.
The pitch envelope's initial frequency is capped at 8 kHz, while A1 selects the
base pitch across its full configured range.

Input-related defaults are stored in the `AppConfig::AudioInput` and
`AppConfig::HitDetection` sections of the same file:

- `AppConfig::AudioInput::CHANNEL` — selected LINE IN channel.
- `AppConfig::AudioInput::GAIN_CODE` — ES8388 input PGA gain.
- `TRIGGER_PRE_THRESHOLD = 180` — starts validation when a magnitude reaches
  this level.
- `TRIGGER_VALIDATION_SAMPLES = 8` — window length, including the candidate
  sample (about 0.18 ms at 44.1 kHz).
- `TRIGGER_MIN_ACTIVE_SAMPLES = 8` — minimum number of samples needed to reject
  the single-spike shape.
- `TRIGGER_FOLLOW_THRESHOLD = 100` — magnitude at which a window sample counts
  as active.
- `TRIGGER_REARM_THRESHOLD = 120` — a complete input block must stay below this
  value before another accepted hit can trigger the voice.
- `PAD_INPUT_MIN` and `PAD_INPUT_MAX` — programmed calibration points for
  velocity mapping.
- `VELOCITY_EFFECTIVE_MAX_*` and `VELOCITY_CURVE_*` — endpoints of the
  sensitivity-dependent velocity response.

## Run

The default PlatformIO environment builds the ESP32 firmware. The native
environment runs the audio signal-path tests described below.

Build and upload the firmware:

```sh
pio run -t upload
```

To use the serial output, select the board serial port and use `115200` baud.
For Teleplot peak monitoring, enable
`AppConfig::Diagnostics::LOG_AUDIO_PEAKS`. Keep
`AppConfig::Diagnostics::LOG_TRIGGER_VALIDATION` enabled while tuning the
filter, then disable either stream independently if it is no longer needed.

## Audio signal-path tests

The native integration test exercises the complete hardware-independent audio
path: a simulated pad impulse enters through the `AudioIo` boundary, then the
real hit detector, synth voice, and output limiter generate the captured mono
output. It verifies rejection of a single-sample spike, acceptance of a light
pad tail, validation across an input-block boundary, and lockout behavior.
Three additional cases use weak, medium, and maximum pad peaks with sensitivity
`0.5`, oscillator pitch `150 Hz`, pitch drop `1 octave`, click disabled, and
full amplitude-velocity response. Focused voice tests also verify click silence,
decay/retrigger behavior, both ends of the AMP VEL interpolation, and equal
maximum levels with AMP VEL at minimum and maximum.

Run the tests from the project root:

```sh
pio test -e native
```

Each executed scenario writes a self-contained SVG waveform plot to
`test/artifacts/`. Filenames contain both the hit strength and simulated input
peak, for example `audio_path_weak_pad_hit_peak_300.svg`. The plots include the
input peak, synthesis parameters (including CLICK and AMP VEL), output peak,
normalized amplitude, and time axis. The click decay test additionally writes
`audio_path_click_envelope_level_1.svg`, a 12 ms close-up of the isolated click
at maximum level. The mixed-signal test writes
`audio_path_mixed_click_and_voice_pad_hit_peak_1560.svg`, a 20 ms close-up of
the click mixed with the regular 150 Hz voice before output. The plots are
regenerated and overwritten on every test run.
