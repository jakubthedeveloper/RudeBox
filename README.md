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
- streams the measured input peak to Teleplot as `>peak:<value>`.

## Sound parameters

The current sound parameters are compile-time defaults in
`include/app_config.h`. Physical potentiometers or encoders will be added later to control them directly.

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

Input-related defaults are stored in the same file:

- `INPUT_CHANNEL` — selected LINE IN channel.
- `INPUT_GAIN_CODE` — ES8388 input PGA gain.
- `MIN_HIT_PEAK` — trigger threshold.
- `MAX_HIT_PEAK` — upper limit used when calculating velocity.

## Run

Build and upload the firmware:

```sh
pio run -t upload
```

To view input peaks, close the PlatformIO Serial Monitor, open Teleplot, select
the board serial port, and use `115200` baud.
