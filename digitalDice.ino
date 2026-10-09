#include <Arduino.h>
#include <TM1637Display.h>

// ---------------- Pin configuration ----------------
#define CLK               2
#define DIO               3
#define BUTTON_IN         7   // rolls the dice
#define SOUND_POT_PIN     A2  // potentiometer wiper: selects rolling-sound style
#define SPEAKER_OUT       9

// Bit-transfer delay for the TM1637 protocol (microseconds), NOT a "test" value
#define TM1637_BIT_DELAY   100

// ---------------- Button debounce ----------------
#define DEBOUNCE_MS   30

// ---------------- Spin animation timing ----------------
// Pulled to file scope (not just local to spinAnimation) so playSoundFrame()
// can compute how far through the whole animation it is, for styles that
// need a smooth sweep across the entire roll rather than per-frame steps.
const int SPIN_TOTAL_STEPS     = 10;
const int SPIN_FRAMES_PER_STEP = 6;
const int SPIN_TOTAL_FRAMES    = SPIN_TOTAL_STEPS * SPIN_FRAMES_PER_STEP; // 60

// ---------------- Sound styles ----------------
//  0 = silent
//  1 = ascending sweep
//  2 = dice-rattle (random short clicks)
//  3 = descending arcade sweep
//  4 = alternating two-tone clack
//  5 = smooth rising siren
//  6 = chiptune arpeggio
//  7 = laser zap (fast descending per step)
//  8 = bell / chime (one note per step)
//  9 = robot beep-boop (slow alternating pitch)
// 10 = casino jackpot trill
// 11 = wind-up spring (stepped rising pitch)
// 12 = xylophone (random pentatonic notes)
// 13 = sci-fi power-up (wide smooth rise)
// 14 = typewriter clack + end ding
// 15 = cymbal hiss / shimmer
#define NUM_SOUND_MODES 16
int soundMode = 2; // updated continuously from the potentiometer position

// ---------------- Display segment frames for the spin animation ----------------
// Each row is one rotating "spinner" frame (a single lit segment moving across
// all 4 digit positions at once), replacing six near-duplicate arrays from an
// earlier version.
const uint8_t spinnerFrames[6][4] = {
  { SEG_A, SEG_B, SEG_C, SEG_D },
  { SEG_B, SEG_C, SEG_D, SEG_E },
  { SEG_C, SEG_D, SEG_E, SEG_F },
  { SEG_D, SEG_E, SEG_F, SEG_A },
  { SEG_E, SEG_F, SEG_A, SEG_B },
  { SEG_F, SEG_A, SEG_B, SEG_C }
};

TM1637Display display(CLK, DIO, TM1637_BIT_DELAY);

int brightness = 3;
int dice1 = 1, dice2 = 1;   // start at a valid value so power-up doesn't show 0/0

// Small helper struct so both buttons share the same debounce/edge-detect logic
struct DebouncedButton {
  uint8_t pin;
  bool lastReading;
  bool actionTaken;
  unsigned long lastDebounceTime;
};

DebouncedButton rollButton = { BUTTON_IN, false, false, 0 };

// Smoothed potentiometer reading, used to pick the sound style. A simple
// low-pass filter (weighted running average) keeps a noisy ADC reading from
// making soundMode flicker between two values near a threshold.
int smoothedPot = 512; // start at the middle of the 0-1023 ADC range

// Most potentiometers don't electrically sweep the full ideal 0-1023 ADC
// range in practice (wiring resistance, wiper contact, unit tolerance). These
// track the REAL observed min/max as the knob gets used, so "fully one way"
// and "fully the other way" always reach mode 0 and the last mode - whatever
// the pot's actual range turns out to be. Sweep the knob fully a couple of
// times after flashing so these settle to the true endpoints.
int potMin = 1023;
int potMax = 0;

// Hysteresis margin around the CURRENT mode's zone, as a fraction of that
// zone's width (not a fixed ADC count) - a reading has to clearly leave the
// current zone by more than this before a change is even considered, which
// stops noise sitting right on a boundary from flip-flopping between modes.
// A FIXED count margin can end up larger than a zone itself if the pot's
// real calibrated range is narrower than expected, which makes the
// outermost zones (mode 0, mode NUM_SOUND_MODES-1) unreachable - scaling by
// zone width avoids that regardless of how wide the pot's actual range is.
const float POT_ZONE_MARGIN_FRACTION = 0.25f;

// The mapped mode value has to hold steady for this long before it's
// actually applied. Without this, tiny electrical noise near a zone
// boundary (e.g. coupled in from the display's CLK/DIO lines) can nudge the
// reading back and forth across the boundary and make the mode flicker
// between two values with the dial completely untouched.
const unsigned long POT_SETTLE_MS = 150;
int candidateMode = 2;
unsigned long candidateSince = 0;

// While millis() is before this timestamp, the center two digits show the
// current sound mode; once it passes, they go blank and only the dice show.
// Set 2000ms into the future whenever the mode changes (and once at boot).
unsigned long modeDisplayUntil = 0;
const unsigned long MODE_DISPLAY_MS = 2000;

// Returns true exactly once per fresh press (after debounce), false otherwise.
// With INPUT_PULLUP, the pin reads LOW when pressed.
bool checkPressed(DebouncedButton &btn) {
  bool reading = (digitalRead(btn.pin) == LOW);
  bool firedThisCall = false;

  if (reading != btn.lastReading) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_MS) {
    if (reading && !btn.actionTaken) {
      firedThisCall = true;
      btn.actionTaken = true;
    } else if (!reading) {
      btn.actionTaken = false;
    }
  }

  btn.lastReading = reading;
  return firedThisCall;
}

// Call once in setup(), after pinMode(), to sync a button's debounce state
// with its REAL electrical state at boot. Without this, the struct's
// hard-coded "not pressed" default can mismatch the true pin voltage during
// power-up/reset, which gets misread as a fresh press on the very first loop.
void initButtonState(DebouncedButton &btn) {
  bool reading = (digitalRead(btn.pin) == LOW);
  btn.lastReading = reading;
  btn.actionTaken = reading; // if it's already held down at boot, don't fire
  btn.lastDebounceTime = millis();
}

// ---------------- Sound helpers ----------------
void playSoundFrame(int frame, int step) {
  int combined = step * SPIN_FRAMES_PER_STEP + frame; // 0..(SPIN_TOTAL_FRAMES-1)

  switch (soundMode) {
    case 0:  // silent
      noTone(SPEAKER_OUT);
      break;

    case 1:  // ascending sweep
      if (frame % 2 == 0) {
        tone(SPEAKER_OUT, 200 + step * 40, 16);
      } else {
        noTone(SPEAKER_OUT);
      }
      break;

    case 2:  // dice-rattle: short random-pitched clicks
      tone(SPEAKER_OUT, random(300, 1200), 12);
      break;

    case 3:  // descending arcade sweep
      tone(SPEAKER_OUT, 1800 - step * 60, 16);
      break;

    case 4:  // alternating two-tone clack
      tone(SPEAKER_OUT, (frame % 2 == 0) ? 440 : 660, 20);
      break;

    case 5: { // smooth rising siren across the whole animation
      int freq = 300 + (int)(700L * combined / (SPIN_TOTAL_FRAMES - 1));
      tone(SPEAKER_OUT, freq, 18);
      break;
    }

    case 6: { // chiptune arpeggio
      const int notes[4] = { 262, 330, 392, 523 };
      tone(SPEAKER_OUT, notes[frame % 4], 14);
      break;
    }

    case 7: // laser zap: fast descending sweep within each step
      tone(SPEAKER_OUT, 1200 - frame * 150, 10);
      break;

    case 8: // bell / chime: one note per step
      if (frame == 0) {
        tone(SPEAKER_OUT, 600 + step * 30, 80);
      } else {
        noTone(SPEAKER_OUT);
      }
      break;

    case 9: // robot beep-boop: slow alternating pitch by step
      tone(SPEAKER_OUT, (step % 2 == 0) ? 300 : 900, 60);
      break;

    case 10: // casino jackpot trill
      tone(SPEAKER_OUT, 500 + frame * 80, 10);
      break;

    case 11: // wind-up spring: pitch steps up once per step, not per frame
      tone(SPEAKER_OUT, 300 + step * 70, 16);
      break;

    case 12: { // xylophone: random pentatonic notes
      const int notes[5] = { 392, 440, 494, 587, 659 };
      tone(SPEAKER_OUT, notes[random(0, 5)], 20);
      break;
    }

    case 13: { // sci-fi power-up: wide smooth rise
      int freq = 150 + (int)(1850L * combined / (SPIN_TOTAL_FRAMES - 1));
      tone(SPEAKER_OUT, freq, 18);
      break;
    }

    case 14: // typewriter clack + end ding
      if (step == SPIN_TOTAL_STEPS - 1 && frame == SPIN_FRAMES_PER_STEP - 1) {
        tone(SPEAKER_OUT, 1800, 100);
      } else {
        tone(SPEAKER_OUT, 200, 8);
      }
      break;

    case 15: // cymbal hiss / shimmer
      tone(SPEAKER_OUT, random(2500, 6000), 4);
      break;
  }
}

// Short confirmation chirp whenever the potentiometer moves into a new
// sound-mode zone, plus a print so you can see which mode you landed on
// over Serial (the number is also always visible on the dice display now).
void confirmSoundModeChange() {
  Serial.print("Sound mode -> ");
  Serial.println(soundMode);

  tone(SPEAKER_OUT, 700 + soundMode * 90, 60);
  delay(70);
  noTone(SPEAKER_OUT);
}

// Reads the potentiometer, smooths it, and maps it onto the sound-mode
// range using the auto-calibrated potMin/potMax. A newly-mapped value only
// becomes a candidate once the reading has clearly left the CURRENT mode's
// zone (past POT_ZONE_MARGIN), and only becomes the active soundMode once
// that candidate has held steady for POT_SETTLE_MS. Together these stop
// noise sitting right on a zone boundary from endlessly flip-flopping
// between two adjacent modes.
void updateSoundModeFromPot() {
  int raw = analogRead(SOUND_POT_PIN);
  smoothedPot = (smoothedPot * 7 + raw) / 8; // heavier low-pass than before

  if (smoothedPot < potMin) potMin = smoothedPot;
  if (smoothedPot > potMax) potMax = smoothedPot;
  if (potMax <= potMin) return; // not enough range observed yet

  int mapped = map(smoothedPot, potMin, potMax, 0, NUM_SOUND_MODES - 1);
  mapped = constrain(mapped, 0, NUM_SOUND_MODES - 1);

  float zoneWidth = (float)(potMax - potMin) / NUM_SOUND_MODES;
  int margin = (int)(zoneWidth * POT_ZONE_MARGIN_FRACTION);
  int zoneLow  = potMin + (int)(soundMode * zoneWidth) - margin;
  int zoneHigh = potMin + (int)((soundMode + 1) * zoneWidth) + margin;

  if (smoothedPot < zoneLow || smoothedPot > zoneHigh) {
    if (mapped != candidateMode) {
      candidateMode = mapped;
      candidateSince = millis();
    }
  } else {
    candidateMode = soundMode; // still within current zone's margin - cancel any pending change
  }

  if (candidateMode != soundMode && (millis() - candidateSince) > POT_SETTLE_MS) {
    soundMode = candidateMode;
    confirmSoundModeChange();
    modeDisplayUntil = millis() + MODE_DISPLAY_MS;
  }
}

// ---------------- Core logic ----------------
void playerRoll(int *die1, int *die2) {
  *die1 = random(1, 7);
  *die2 = random(1, 7);

  Serial.print("Rolled: ");
  Serial.print(*die1);
  Serial.print(" and ");
  Serial.println(*die2);
}

void spinAnimation() {
  for (int step = 0; step < SPIN_TOTAL_STEPS; step++) {
    for (int frame = 0; frame < SPIN_FRAMES_PER_STEP; frame++) {
      display.setSegments(spinnerFrames[frame]);
      playSoundFrame(frame, step);
      delay(16); // brief pause so the spin is visible/audible
    }
  }
  noTone(SPEAKER_OUT);
  display.clear();
}

// Uses all 4 digits of the TM1637: die 1 on the far left, die 2 on the far
// right, always. The two center digits show the current sound mode (0-20,
// with a leading zero so it fills both positions) only for MODE_DISPLAY_MS
// after it last changed - otherwise they're left blank so just the dice show.
void displayNum(int num, int num2, int mode) {
  display.setBrightness(brightness, true);
  display.showNumberDecEx(num, 0, false, 1, 0);   // position 0: die 1

  if (millis() < modeDisplayUntil) {
    display.showNumberDecEx(mode, 0, true, 2, 1); // positions 1-2: sound mode
  } else {
    uint8_t blank[2] = { 0, 0 };
    display.setSegments(blank, 2, 1);             // positions 1-2: blank
  }

  display.showNumberDecEx(num2, 0, false, 1, 3);  // position 3: die 2
}

void setup() {
  pinMode(SPEAKER_OUT, OUTPUT);
  pinMode(BUTTON_IN, INPUT_PULLUP); // internal pull-up: button shorts pin to GND when pressed
  // SOUND_POT_PIN needs no pinMode() call - analogRead() handles analog pins directly.

  Serial.begin(9600);
  display.clear();

  initButtonState(rollButton);
  modeDisplayUntil = millis() + MODE_DISPLAY_MS; // show the startup sound mode briefly too

  randomSeed(analogRead(A1)); // seed ONCE at startup, not on every roll
}

void loop() {
  if (checkPressed(rollButton)) {
    Serial.println("Roll button pressed - rolling");
    spinAnimation();
    playerRoll(&dice1, &dice2);
  }

  updateSoundModeFromPot();
  displayNum(dice1, dice2, soundMode);
}
