# ESP32-A1S / ES8388 — impulsy LINE IN w Teleplot

Projekt realizuje tylko jeden tor:

`pad -> LINE IN -> ES8388 -> I2S -> peak bloku -> Teleplot`

ES8388 używa wejścia LINE2. Audio jest odbierane jako stereo 16-bit/44,1 kHz,
ale analizowany jest tylko kanał wybrany w `src/main.cpp`. Domyślnie jest to
kanał prawy. Peak jest wyznaczany co 256 ramek, czyli około 172 razy na sekundę.

```text
>peak:123
```

## Struktura

- `src/main.cpp` — inicjalizacja i wysyłanie danych Teleplot,
- `src/es8388.cpp` — konfiguracja kodeka i wejścia LINE2,
- `src/audio_input.cpp` — odbiór I2S i obliczanie peaków,
- `include/*.h` — małe interfejsy obu modułów.

## Uruchomienie

```sh
pio run -t upload
```

Następnie zamknij Serial Monitor, otwórz Teleplot i wybierz port płytki oraz
`115200` baud.

Wzmocnienie wejścia ustawia `INPUT_GAIN_CODE` w `src/es8388.cpp`:
`0=0 dB`, `1=3 dB`, `2=6 dB`, `3=9 dB`, `4=12 dB`.

Kanał wejściowy wybiera stała w `src/main.cpp`:

```cpp
constexpr AudioInput::Channel INPUT_CHANNEL = AudioInput::Channel::Right;
```

Możliwe wartości to `AudioInput::Channel::Right` i
`AudioInput::Channel::Left`.
