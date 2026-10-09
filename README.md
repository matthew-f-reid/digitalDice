# Digital Dice (Arduino Nano)

A two-dice roller built on an Arduino Nano. Press the button and the display spins, a sound plays, then two dice values (1–6) are shown. A potentiometer picks one of 16 rolling-sound styles, including silent.

## Features

- Rolls two dice per button press, with a spinning animation on the 4-digit display
- 16 selectable rolling-sound styles (mode 0 is silent), chosen with a dial
- Dice values shown on the outer two display digits
- Selected sound mode shown on the center two digits for 2 seconds after it changes, then hidden
- Debounced button, so one press means one roll even if the button is held
- Smoothed, auto-calibrated potentiometer with hysteresis, so the mode doesn't flicker

## Hardware

| Part | Notes |
|---|---|
| Arduino Nano | |
| TM1637 4-digit 7-segment display | Shows dice and sound mode |
| Push button | Roll trigger |
| Potentiometer (e.g. 10 kΩ) | Sound style dial |
| Small speaker | Driven directly from a pin (see notes) |
| 4×AA battery holder (NiMH) | Optional, for portable use |

## Wiring

| Component | Connection |
|---|---|
| TM1637 `CLK` | D2 |
| TM1637 `DIO` | D3 |
| TM1637 `VCC` / `GND` | 5V / GND |
| Roll button | One leg to **D7**, other leg to **GND** |
| Speaker | One wire to **D9**, other to **GND** |
| Potentiometer wiper (middle leg) | **A2** |
| Potentiometer outer legs | 5V and GND |
| A1 | Leave **unconnected** (floating noise is used to seed the random generator) |

Notes:

- The button uses the internal pull-up (`INPUT_PULLUP`), so no external resistor is needed. It reads LOW when pressed.
- Use a **star ground**: run every GND connection back to one point near the Nano's GND pin. Sharing a ground path between the display and the speaker caused an audible hum during development.
- A 1.5 W speaker wired straight to a pin works at low volume, but a pin is only rated for about 20 mA. For louder or longer-term use, add a small amplifier module between the pin and the speaker.

## Software setup

1. Install the Arduino IDE.
2. In the Library Manager, install the **TM1637** library by Avishay Orpaz (provides `TM1637Display`).
3. Put `digitalDice.ino` in a folder with the same name (`digitalDice/`). The IDE compiles every `.ino` in a sketch folder together, so keep only one there.
4. Select **Arduino Nano** and the right processor and port, then upload.

## Using it

- **Roll:** press the button. The display spins while the selected sound plays, then both dice show.
- **Change sound:** turn the dial. The new mode number appears on the center two digits for 2 seconds (it also appears for 2 seconds at power-up) and a short confirmation chirp plays.
- **Calibrate the dial:** after flashing, sweep the pot fully to both ends a couple of times. The sketch learns the pot's real min and max as you use it, so the first and last modes become reachable.

### Display layout

```
[ die 1 ] [ mode tens ] [ mode ones ] [ die 2 ]
```

While rolling, the spinner animation takes over all four digits. The mode digits only appear for 2 seconds after a change.

### Sound modes

| Mode | Style |
|---|---|
| 0 | Silent |
| 1 | Ascending sweep |
| 2 | Dice rattle (random short clicks) |
| 3 | Descending arcade sweep |
| 4 | Alternating two-tone clack |
| 5 | Smooth rising siren |
| 6 | Chiptune arpeggio |
| 7 | Laser zap |
| 8 | Bell / chime |
| 9 | Robot beep-boop |
| 10 | Casino jackpot trill |
| 11 | Wind-up spring (stepped rising pitch) |
| 12 | Xylophone (random pentatonic notes) |
| 13 | Sci-fi power-up |
| 14 | Typewriter clack with end ding |
| 15 | Cymbal hiss / shimmer |

## Serial output

9600 baud. Useful for debugging:

```
Roll button pressed - rolling
Rolled: 3 and 5
Sound mode -> 7
```

## Tunable settings

All near the top of `digitalDice.ino`:

| Setting | Default | What it does |
|---|---|---|
| `DEBOUNCE_MS` | 30 | Button debounce time |
| `brightness` | 3 | Display brightness (0–7) |
| `SPIN_TOTAL_STEPS` / `SPIN_FRAMES_PER_STEP` | 10 / 6 | Length of the roll animation |
| `MODE_DISPLAY_MS` | 2000 | How long the sound mode stays on screen |
| `POT_SETTLE_MS` | 150 | How long a new dial position must hold before it takes effect |
| `POT_ZONE_MARGIN_FRACTION` | 0.25 | Hysteresis around each mode zone (fraction of zone width) |
| `TM1637_BIT_DELAY` | 100 | TM1637 protocol timing in microseconds |

## Power

Estimated draw is roughly 100–150 mA on average, so a 6-hour session needs about 900–1200 mAh. A 4×AA NiMH pack (2000 mAh or more) covers that comfortably. A 9V alkaline battery is a poor fit: it sags under the speaker's current spikes and gives far less usable capacity at this draw.

## Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| Compile errors about duplicate `setup()`, `loop()` or variables | More than one `.ino` in the sketch folder. Keep one, named to match the folder |
| Constant hum from the speaker | Shared ground path with the display. Use a star ground |
| Dial mode flickers on its own | ADC noise near a zone boundary. The settle timer and hysteresis handle most of it. For the rest, add a 0.1 µF ceramic capacitor from A2 to GND and keep the pot wire away from the CLK/DIO wires |
| First or last sound mode hard to reach | Sweep the dial fully to both ends so the auto-calibration learns its real range |
| Rolls with no button press at boot | The button should short D7 to GND when pressed. If it is wired to 5V instead, the logic needs inverting |
