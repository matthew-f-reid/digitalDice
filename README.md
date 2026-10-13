# digitalDice (Arduino Nano)

A two-dice roller built on an Arduino Nano. Press the button and the display spins, a sound plays, then two dice values (1–6) are shown. A potentiometer picks one of 16 rolling-sound styles, including silent. Each roll is also sent to a PC, saved to MongoDB, and the PC sends back the time and date, which the display shows after the roll.

## Features

- Rolls two dice per button press, with a spinning animation on the 4-digit display
- 16 selectable rolling-sound styles (mode 0 is silent), chosen with a dial
- Dice values shown on the outer two display digits
- Selected sound mode shown on the center two digits for 2 seconds after it changes, then hidden
- Debounced button, so one press means one roll even if the button is held
- Smoothed, auto-calibrated potentiometer with hysteresis, so the mode doesn't flicker
- Sends every roll, with the selected sound mode, to a PC over USB serial, where a Python script saves it to MongoDB
- The PC replies with the time and date of the roll, which the display shows for a few seconds after each roll

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
4. Select **Arduino Nano** and the right port. Most clone Nanos need **Tools > Processor > ATmega328P (Old Bootloader)**; with the wrong setting, uploads fail even though serial works.
5. Upload.

## Using it

- **Roll:** press the button. The display spins while the selected sound plays, then both dice show.
- **Change sound:** turn the dial. The new mode number appears on the center two digits for 2 seconds (it also appears for 2 seconds at power-up) and a short confirmation chirp plays.
- **Timestamp:** with `diceLogger.py` running, the PC replies to each roll with the time and date. After the dice appear, the display shows the time (`HH:MM`) for 2 seconds, then the date (`DDMM`, for example `1210` for 12 October) for 2 seconds, then returns to the dice. The Nano's onboard LED also blinks when a timestamp arrives. Without the script running, nothing is shown and the dice stay on screen. Pressing the button again cancels a timestamp that is still showing.
- **Calibrate the dial:** after flashing, sweep the pot fully to both ends a couple of times. The sketch learns the pot's real min and max as you use it, so the first and last modes become reachable.

### Display layout

```
[ die 1 ] [ mode tens ] [ mode ones ] [ die 2 ]
```

While rolling, the spinner animation takes over all four digits. The mode digits only appear for 2 seconds after a change.

During the timestamp display, all four digits show the time, then the date:

```
[ H ] [ H ] [ M ] [ M ]    then    [ D ] [ D ] [ M ] [ M ]
```

If your display module has a colon, it lights between the hour and minute digits; otherwise a dot appears after the second digit. The display is only rewritten when something visible changes, which keeps the loop fast so the dial responds without lag.

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

## Serial protocol

9600 baud, newline-terminated lines, in both directions.

**Nano to PC**

| Line | When | Used by the logger |
|---|---|---|
| `ROLL,3,5,7` | After every roll: die 1, die 2, selected sound mode | Yes, saved to MongoDB |
| `Sound mode -> 7` | Whenever the dial moves into a new mode | Printed only |
| `Roll button pressed - rolling` | Start of a roll | Printed only |
| `RX,2026-10-12 18:23:45` | Echo of every line the Nano receives from the PC | Printed only (confirms the timestamp arrived) |

**PC to Nano**

```
2026-10-12 18:23:45
```

Sent right after a roll is saved. It is the PC's local time in `YYYY-MM-DD HH:MM:SS` form. The sketch ignores any received line that doesn't match that shape.

## Logging rolls to MongoDB (PC side)

`diceLogger.py` reads the `ROLL` lines from the Nano over USB serial, saves each roll into a local MongoDB database, and sends the roll's timestamp back to the Nano.

### Requirements

- Python 3 with `pyserial` and `pymongo`:

  ```
  pip install pyserial pymongo
  ```

  This works from any folder and installs globally. If `pip` isn't recognised on Windows, use `python -m pip install pyserial pymongo`. To keep the packages scoped to this project, create a virtual environment first (`python -m venv venv`, then `venv\Scripts\activate`).
- MongoDB running on the PC (default `mongodb://localhost:27017`). MongoDB Compass is handy for viewing the data.
- The current sketch uploaded to the Nano. It must print `ROLL,x,y,mode`. An older sketch that prints only `ROLL,x,y` still saves rolls, but without the sound fields. The oldest format (`Rolled: x and y`) is ignored.

### Setup and run

1. Open `diceLogger.py` and set `SERIAL_PORT` to the Nano's port (Arduino IDE > Tools > Port, e.g. `COM6`). The number can change if you plug into a different USB port.
2. Close the Arduino Serial Monitor. Only one program can use the port at a time, so also stop the script before uploading a new sketch.
3. Run:

   ```
   python diceLogger.py
   ```

   It prints `Listening on COM6 -> digitalDice.rolls`. Opening the port resets the Nano, so it waits 2 seconds before listening.
4. Press the roll button. The script prints the saved roll, for example `Saved roll: 3 + 5 = 8 (sound 7: Laser zap)`, and the display then shows the time and date. Stop with Ctrl+C.

### What gets stored

Database `digitalDice`, collection `rolls`. One document per roll:

```
{
  die1: 3, die2: 5, total: 8,
  soundMode: 7, soundName: "Laser zap",
  rolledAt: <local date and time>
}
```

- `soundName` comes from the `SOUND_NAMES` list at the top of `diceLogger.py`. Update that list if you change the sound modes in the sketch.
- If the roll line carries no mode (an older sketch), the roll is saved without the two sound fields.
- `rolledAt` is saved in the PC's **local** time. MongoDB dates carry no timezone, so Compass labels it with a trailing `Z` (UTC) even though it is local time. Rolls saved before this change are in UTC.
- The database and collection are created on the first insert. In MongoDB Compass, click the refresh button to see new rolls; it doesn't update automatically.

### Script settings

At the top of `diceLogger.py`:

| Setting | Default | What it does |
|---|---|---|
| `SERIAL_PORT` | `COM6` | Port the Nano is on |
| `BAUD_RATE` | 9600 | Must match `Serial.begin()` in the sketch |
| `MONGO_URI` | `mongodb://localhost:27017` | MongoDB server address |
| `DB_NAME` / `COLLECTION_NAME` | `digitalDice` / `rolls` | Where rolls are saved |
| `SOUND_NAMES` | 16 names | Maps a mode number to the name stored with each roll |

## Tunable settings

All near the top of `digitalDice.ino`:

| Setting | Default | What it does |
|---|---|---|
| `DEBOUNCE_MS` | 30 | Button debounce time |
| `brightness` | 3 | Display brightness (0–7) |
| `SPIN_TOTAL_STEPS` / `SPIN_FRAMES_PER_STEP` | 10 / 6 | Length of the roll animation |
| `MODE_DISPLAY_MS` | 2000 | How long the sound mode stays on screen |
| `STAMP_PHASE_MS` | 2000 | How long the time, and then the date, stay on screen after a roll |
| `POT_SAMPLE_MS` | 5 | How often the dial is read |
| `POT_SETTLE_MS` | 60 | How long a new dial position must hold before it takes effect. Raise it if the mode flickers, lower it if the dial feels slow |
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
| Upload fails (`stk500_getsync` or "programmer is not responding") but serial works | Wrong bootloader setting. Select Processor > ATmega328P (Old Bootloader). Also disconnect the battery and close the Serial Monitor |
| Python script saves nothing after a roll | The board has an old sketch printing `Rolled: x and y`. Re-upload the current sketch. Check `SERIAL_PORT` matches the Nano |
| Script exits with `Can't open COMx` | Wrong port, or the Serial Monitor or another program is holding it |
| Script exits with `Can't reach MongoDB` | MongoDB isn't running on the PC, or `MONGO_URI` is wrong |
| "Saved roll" prints but nothing shows in Compass | Click refresh in Compass and check you're connected to the same server, database `digitalDice`, collection `rolls` |
| Saved roll has no `soundMode` or `soundName` | The board is running an older sketch that doesn't send the mode. Upload the current sketch |
| Time and date don't appear on the display after a roll | Check the script is running and prints `RX,<timestamp>` after the roll. If `RX,` doesn't appear, the Nano isn't receiving (old sketch, or wrong port). If it appears and the display doesn't change, check the sketch is the current one |
| Displayed time is wrong | The time comes from the PC's clock. Check the PC's date, time and timezone |
