# Synth Tom ESP32

Synth Tom ESP32 is an electronic drum synth prototype for the ESP32 Audio Kit
V2.2 with the ESP32-A1S module and ES8388 audio codec.

The firmware currently:

- reads drum pad impulses from the right channel of ES8388 LINE IN,
- ignores peaks below `120` and limits peaks above `3000`,
- converts the accepted peak range into normalized velocity,
- triggers one monophonic synth-drum voice per hit,
- generates a triangle waveform with amplitude and pitch envelopes,
- sends the same synthesized signal to the left and right audio outputs,
- streams the measured input peak to Teleplot as `>peak:<value>`,
- reads potentiometer channel 0 from an ADS7830 every second,
- briefly lights the activity LED after a drum-pad hit is detected.

## Code structure

The firmware is organized from general application flow to hardware and signal
processing details:

- `main.cpp` only enters `Application::begin()` and `Application::update()`.
- `application.cpp` coordinates the audio path and user interface.
- `synth_engine.cpp` shows the complete synthesis path: read the pad input,
  detect and trigger a hit, render the voice, and write the output block.
- `synth_voice.cpp` defines the sound of one voice. Its public `trigger()` and
  `render()` functions delegate envelope, pitch, phase, and stereo-buffer work
  to focused private helpers.
- `waveforms.cpp` owns the reusable waveform generator; it is independent of a
  particular synth voice.
- `user_interface.cpp` owns potentiometer polling, UI logging, and LED timing.
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

With the `Ext Com` jumper closed, COM is connected on the board and must not
also be wired externally to GND in this setup. The 10kΩ potentiometer is used
as a voltage divider between 3.3V and GND, with its wiper connected through a
1kΩ series resistor to ADS7830 channel A0.

The number of active channels is set by
`AppConfig::Ui::POTENTIOMETER_COUNT` in `include/app_config.h`. The driver
supports all eight ADS7830 channels; only channel 0 is enabled by default. Each
reported value is the average of 16 raw 8-bit samples and is printed with two
decimal places on the 0 to 255 scale at 115200 baud. Averaging makes noisy
readings more stable, but does not increase the native resolution of the
ADS7830.

### Activity LED

The activity LED is output-only feedback. Drum hits are detected from the
dynamic pad connected to ES8388 LINE IN; the LED does not participate in hit
detection. By default it is configured as active-low on GPIO22 and stays on for
40 ms after a detected hit. These values are in the `AppConfig::Ui` section of
`include/app_config.h`.

## Logging

Serial output categories can be enabled independently in the
`AppConfig::Diagnostics` and `AppConfig::Ui` sections of
`include/app_config.h`:

- `AppConfig::Diagnostics::LOG_AUDIO_PEAKS` — streams ES8388 input peaks in
  Teleplot format,
- `AppConfig::Ui::LOG_POTENTIOMETERS` — prints raw ADS7830 readings.

By default only potentiometer logging is enabled. Fatal initialization errors
are always printed.

## Sound parameters

The current sound parameters are compile-time defaults in the
`AppConfig::Voice` section of `include/app_config.h`. Physical potentiometers
or encoders will be added later to control them directly.

- `BASE_FREQUENCY_HZ` — oscillator frequency after the pitch envelope reaches zero.
- `PITCH_SWEEP_HZ` — maximum frequency offset added at the beginning of a hit.
- `AMP_RELEASE_MS` — duration of the amplitude envelope and therefore the sound.
- `PITCH_DECAY_MS` — time taken by the pitch envelope to fall to the base
  frequency.
- `MIN_VOLUME` — output level assigned to the weakest accepted hit.
- `AMP_VELOCITY_AMOUNT` — influence of hit velocity on output amplitude;
  `0.0` disables the influence and `1.0` applies the full velocity range.
- `PITCH_VELOCITY_AMOUNT` — influence of hit velocity on the starting pitch;
  `0.0` keeps the full pitch sweep fixed and `1.0` makes it fully velocity
  dependent.

Input-related defaults are stored in the `AppConfig::AudioInput` and
`AppConfig::HitDetection` sections of the same file:

- `AppConfig::AudioInput::CHANNEL` — selected LINE IN channel.
- `AppConfig::AudioInput::GAIN_CODE` — ES8388 input PGA gain.
- `AppConfig::HitDetection::MIN_HIT_PEAK` — trigger threshold.
- `AppConfig::HitDetection::MAX_HIT_PEAK` — upper limit used when calculating
  velocity.

## Run

Build and upload the firmware:

```sh
pio run -t upload
```

To use the serial output, select the board serial port and use `115200` baud.
For Teleplot peak monitoring, enable
`AppConfig::Diagnostics::LOG_AUDIO_PEAKS` first.
