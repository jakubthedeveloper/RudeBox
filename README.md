# Synth Tom ESP32

Synth Tom ESP32 is an electronic drum synth prototype for the ESP32 Audio Kit
V2.2 with the ESP32-A1S module and ES8388 audio codec.

The firmware currently:

- reads drum pad impulses from the right channel of ES8388 LINE IN,
- accepts pad peaks at or above the fixed trigger threshold,
- maps accepted peaks into normalized velocity using a sensitivity control,
- triggers one monophonic synth-drum voice per hit,
- generates a triangle waveform with amplitude and pitch envelopes,
- applies output headroom and final brickwall limiting,
- sends the synthesized mono signal to the left audio output,
- streams the measured input peak to Teleplot as `>peak:<value>`,
- reads three potentiometers from an ADS7830 about every 5 ms,
- briefly lights the activity LED after a drum-pad hit is detected.

## Code structure

The firmware is organized from general application flow to hardware and signal
processing details:

- `main.cpp` enters `Application::begin()` and `Application::update()`.
- `application.cpp` coordinates the audio path and user interface, explicitly
  passing the current UI control state to the synth engine.
- `synth_engine.cpp` shows the complete synthesis path: read the pad input,
  detect and trigger a hit, render the voice, and write the output block.
- `hit_detector.cpp` owns hit re-arming and maps accepted pad peaks to velocity
  using the current sensitivity.
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
  it delegates potentiometer details to `synth_control_input.cpp` and owns LED
  timing directly.
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
- A2 — pitch-drop depth from 0 to 4.5 octaves.

All channels are sampled every 5 ms. An EMA filter and a two-count dead zone
stabilize the native 8-bit readings. Pitch uses exponential mapping, while
pitch-drop uses a squared curve for finer control near zero. The fixed trigger
threshold determines which peaks are accepted, and the sensitivity control maps
accepted peaks to velocity. Every control maps a higher ADC reading to a higher
control value.

### Activity LED

The activity LED signals drum hits detected from the dynamic pad connected to
ES8388 LINE IN. By default it is configured as active-low on GPIO22 and stays on
for 40 ms after a detected hit. These values are in the `AppConfig::Ui` section
of `include/app_config.h`.

## Logging

Serial output categories can be enabled independently in the
`AppConfig::Diagnostics` and `AppConfig::Controls` sections of
`include/app_config.h`:

- `AppConfig::Diagnostics::LOG_AUDIO_PEAKS` — streams ES8388 input peaks in
  Teleplot format,
- `AppConfig::Controls::LOG_CONTROL_VALUES` — prints throttled raw, filtered,
  and mapped potentiometer values.

Both diagnostic modes are disabled by default. Fatal initialization errors are
always printed.

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
- `MIN_VOLUME` — output level assigned to the weakest accepted hit.
- `AMP_VELOCITY_AMOUNT` — influence of hit velocity on output amplitude, from
  constant amplitude contribution at `0.0` to the full velocity range at `1.0`.
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
- `AppConfig::HitDetection::PAD_TRIGGER_THRESHOLD` — fixed trigger threshold.
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
`AppConfig::Diagnostics::LOG_AUDIO_PEAKS` first.

## Audio signal-path tests

The native integration test exercises the complete hardware-independent audio
path: a simulated pad impulse enters through the `AudioIo` boundary, then the
real hit detector, synth voice, and output limiter generate the captured mono
output. Three cases use weak, medium, and maximum pad peaks with sensitivity
`0.5`, oscillator pitch `150 Hz`, and pitch drop `1 octave`.

Run the tests from the project root:

```sh
pio test -e native
```

Each executed scenario writes a self-contained SVG waveform plot to
`test/artifacts/`. Filenames contain both the hit strength and simulated input
peak, for example `audio_path_weak_pad_hit_peak_300.svg`. The plots include the
input peak, synthesis parameters, output peak, normalized amplitude, and time
axis. They are regenerated and overwritten on every test run.
