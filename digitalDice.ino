#include <TM1637Display.h>

// ---------------- Pins ----------------
#define CLK            2
#define DIO            3
#define BUTTON_IN      7   // roll button, wired to GND (uses the internal pull-up)
#define SOUND_POT_PIN  A2  // potentiometer wiper: selects the rolling sound
#define SPEAKER_OUT    9

// ---------------- Settings ----------------
#define TM1637_BIT_DELAY 100  // TM1637 protocol timing (microseconds)
#define DEBOUNCE_MS      30
const int brightness = 3;     // display brightness, 0-7

// The roll animation is SPIN_TOTAL_STEPS steps of SPIN_FRAMES_PER_STEP frames (~16 ms each).
const int SPIN_TOTAL_STEPS     = 10;
const int SPIN_FRAMES_PER_STEP = 6;
const int SPIN_TOTAL_FRAMES    = SPIN_TOTAL_STEPS * SPIN_FRAMES_PER_STEP;

const unsigned long MODE_DISPLAY_MS = 2000;  // how long the sound mode number stays on screen
const unsigned long STAMP_PHASE_MS  = 2000;  // after a roll: time, then date, each shown this long

// Dial: sampled every POT_SAMPLE_MS, and a new mode must hold for POT_SETTLE_MS.
// A reading must leave the current mode's zone by POT_ZONE_MARGIN_FRACTION of
// a zone's width before a change is considered (stops flicker on zone edges).
const unsigned long POT_SAMPLE_MS = 5;
const unsigned long POT_SETTLE_MS = 60;
const float POT_ZONE_MARGIN_FRACTION = 0.25f;

// Sound modes: 0 silent, 1 ascending sweep, 2 dice rattle, 3 descending arcade sweep,
// 4 two-tone clack, 5 rising siren, 6 chiptune arpeggio, 7 laser zap, 8 bell,
// 9 robot beep-boop, 10 jackpot trill, 11 wind-up spring, 12 xylophone,
// 13 sci-fi power-up, 14 typewriter + ding, 15 cymbal hiss
#define NUM_SOUND_MODES 16

// ---------------- State ----------------
TM1637Display display(CLK, DIO, TM1637_BIT_DELAY);

int soundMode = 2;
int dice1 = 1, dice2 = 1;
bool displayDirty = true;  // forces a redraw (set whenever the display is wiped)

// Button debounce
bool lastReading, buttonHandled;
unsigned long lastChangeTime;

// Dial. potMin/potMax learn the dial's real range as it is turned, so sweep
// the knob fully after each reset.
int smoothedPot = 512, potMin = 1023, potMax = 0, candidateMode = 2;
unsigned long candidateSince = 0;
unsigned long modeDisplayUntil = 0;

// Time and date sent back by DiceLogger.py, stored as HHMM and DDMM
int stampTime = 0, stampDate = 0;
unsigned long stampStart = 0;
bool stampActive = false;

// ---------------- Button ----------------
// True exactly once per debounced press (the pin reads LOW when pressed).
bool buttonPressed() {
  bool reading = (digitalRead(BUTTON_IN) == LOW);
  bool fired = false;

  if (reading != lastReading) lastChangeTime = millis();

  if (millis() - lastChangeTime > DEBOUNCE_MS) {
    fired = reading && !buttonHandled;
    buttonHandled = reading;
  }

  lastReading = reading;
  return fired;
}

// ---------------- Sound ----------------
// Called once per animation frame: frame is 0-5, step is 0-9.
void playSoundFrame(int frame, int step) {
  int combined = step * SPIN_FRAMES_PER_STEP + frame;  // 0..59, progress through the whole roll

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

    case 2:  // dice rattle: short random clicks
      tone(SPEAKER_OUT, random(300, 1200), 12);
      break;

    case 3:  // descending arcade sweep
      tone(SPEAKER_OUT, 1800 - step * 60, 16);
      break;

    case 4:  // alternating two-tone clack
      tone(SPEAKER_OUT, (frame % 2 == 0) ? 440 : 660, 20);
      break;

    case 5:  // smooth rising siren
      tone(SPEAKER_OUT, 300 + (int)(700L * combined / (SPIN_TOTAL_FRAMES - 1)), 18);
      break;

    case 6: {  // chiptune arpeggio
      const int notes[4] = { 262, 330, 392, 523 };
      tone(SPEAKER_OUT, notes[frame % 4], 14);
      break;
    }

    case 7:  // laser zap: fast descending sweep within each step
      tone(SPEAKER_OUT, 1200 - frame * 150, 10);
      break;

    case 8:  // bell: one note per step
      if (frame == 0) {
        tone(SPEAKER_OUT, 600 + step * 30, 80);
      } else {
        noTone(SPEAKER_OUT);
      }
      break;

    case 9:  // robot beep-boop: alternates every step
      tone(SPEAKER_OUT, (step % 2 == 0) ? 300 : 900, 60);
      break;

    case 10:  // casino jackpot trill
      tone(SPEAKER_OUT, 500 + frame * 80, 10);
      break;

    case 11:  // wind-up spring: pitch climbs in stairs
      tone(SPEAKER_OUT, 300 + step * 70, 16);
      break;

    case 12: {  // xylophone: random pentatonic notes
      const int notes[5] = { 392, 440, 494, 587, 659 };
      tone(SPEAKER_OUT, notes[random(0, 5)], 20);
      break;
    }

    case 13:  // sci-fi power-up: wide smooth rise
      tone(SPEAKER_OUT, 150 + (int)(1850L * combined / (SPIN_TOTAL_FRAMES - 1)), 18);
      break;

    case 14:  // typewriter clacks, then a ding on the last frame
      if (step == SPIN_TOTAL_STEPS - 1 && frame == SPIN_FRAMES_PER_STEP - 1) {
        tone(SPEAKER_OUT, 1800, 100);
      } else {
        tone(SPEAKER_OUT, 200, 8);
      }
      break;

    case 15:  // cymbal hiss
      tone(SPEAKER_OUT, random(2500, 6000), 4);
      break;
  }
}

// ---------------- Dial ----------------
void updateSoundModeFromPot() {
  static unsigned long lastSample = 0;
  if (millis() - lastSample < POT_SAMPLE_MS) return;
  lastSample = millis();

  smoothedPot = (smoothedPot * 3 + analogRead(SOUND_POT_PIN)) / 4;
  if (smoothedPot < potMin) potMin = smoothedPot;
  if (smoothedPot > potMax) potMax = smoothedPot;
  if (potMax <= potMin) return;  // not enough range seen yet

  // Split the usable range (ignoring ~3% at each end) into equal zones, one per mode.
  int trim = (potMax - potMin) / 32;
  int lo = potMin + trim;
  int hi = potMax - trim;
  int usable = hi - lo + 1;
  int pos = constrain(smoothedPot, lo, hi) - lo;
  int mapped = (int)((long)pos * NUM_SOUND_MODES / usable);

  // The current mode's zone, widened by the hysteresis margin.
  float zoneWidth = (float)usable / NUM_SOUND_MODES;
  int margin = (int)(zoneWidth * POT_ZONE_MARGIN_FRACTION);
  int zoneLow  = lo + (int)(soundMode * zoneWidth) - margin;
  int zoneHigh = lo + (int)((soundMode + 1) * zoneWidth) + margin;

  if (smoothedPot < zoneLow || smoothedPot > zoneHigh) {
    if (mapped != candidateMode) {
      candidateMode = mapped;
      candidateSince = millis();
    }
  } else {
    candidateMode = soundMode;  // still inside the current zone: cancel any pending change
  }

  if (candidateMode != soundMode && millis() - candidateSince > POT_SETTLE_MS) {
    soundMode = candidateMode;
    Serial.print("Sound mode -> ");
    Serial.println(soundMode);
    tone(SPEAKER_OUT, 700 + soundMode * 90, 60);  // confirmation chirp
    modeDisplayUntil = millis() + MODE_DISPLAY_MS;
  }
}

// ---------------- Rolling ----------------
void rollDice() {
  dice1 = random(1, 7);
  dice2 = random(1, 7);

  // Read by DiceLogger.py: ROLL,<die1>,<die2>,<sound mode>
  Serial.print("ROLL,");
  Serial.print(dice1);
  Serial.print(",");
  Serial.print(dice2);
  Serial.print(",");
  Serial.println(soundMode);
}

void spinAnimation() {
  for (int step = 0; step < SPIN_TOTAL_STEPS; step++) {
    for (int frame = 0; frame < SPIN_FRAMES_PER_STEP; frame++) {
      // Each digit lights one of the six outer segments (A-F are bits 0-5),
      // shifted by one per digit, so the segments appear to chase around.
      uint8_t segments[4];
      for (int digit = 0; digit < 4; digit++) {
        segments[digit] = 1 << ((frame + digit) % 6);
      }
      display.setSegments(segments);
      playSoundFrame(frame, step);
      delay(16);
    }
  }
  noTone(SPEAKER_OUT);
  display.clear();
  displayDirty = true;
}

// ---------------- Display ----------------
// Dice on the outer digits, the sound mode on the two centre digits for
// MODE_DISPLAY_MS after it changes. After a roll the time, then the date,
// take over all four digits. The display is only rewritten when something
// visible changed, because every redraw is slow (several ms) and would make
// the loop, and so the dial, laggy.
void updateDisplay() {
  static int lastView = -1, lastDie1 = -1, lastDie2 = -1, lastMode = -1;

  // 0 = time, 1 = date, 2 = dice only, 3 = dice + sound mode
  int view = 2;
  if (stampActive) {
    unsigned long elapsed = millis() - stampStart;
    if (elapsed < STAMP_PHASE_MS)          view = 0;
    else if (elapsed < 2 * STAMP_PHASE_MS) view = 1;
    else                                   stampActive = false;
  }
  if (view == 2 && millis() < modeDisplayUntil) view = 3;

  if (!displayDirty && view == lastView && dice1 == lastDie1 && dice2 == lastDie2 &&
      (view != 3 || soundMode == lastMode)) {
    return;
  }

  if (view == 0) {
    display.showNumberDecEx(stampTime, 0b01000000, true, 4, 0);  // HH:MM (colon if the module has one)
  } else if (view == 1) {
    display.showNumberDecEx(stampDate, 0, true, 4, 0);           // DDMM
  } else {
    display.showNumberDecEx(dice1, 0, false, 1, 0);
    if (view == 3) {
      display.showNumberDecEx(soundMode, 0, true, 2, 1);
    } else {
      const uint8_t blank[2] = { 0, 0 };
      display.setSegments(blank, 2, 1);
    }
    display.showNumberDecEx(dice2, 0, false, 1, 3);
  }

  lastView = view;
  lastDie1 = dice1;
  lastDie2 = dice2;
  lastMode = soundMode;
  displayDirty = false;
}

// ---------------- Timestamp from Python ----------------
// Reads one line in the form "YYYY-MM-DD HH:MM:SS".
void checkForIncomingTime() {
  if (Serial.available() == 0) return;

  String incoming = Serial.readStringUntil('\n');
  incoming.trim();

  Serial.print("RX,");  // echo, so DiceLogger.py can show what arrived
  Serial.println(incoming);

  if (incoming.length() < 16 || incoming.charAt(4) != '-' || incoming.charAt(13) != ':') return;

  stampDate = incoming.substring(8, 10).toInt() * 100 + incoming.substring(5, 7).toInt();
  stampTime = incoming.substring(11, 13).toInt() * 100 + incoming.substring(14, 16).toInt();
  stampStart = millis();
  stampActive = true;

  digitalWrite(LED_BUILTIN, HIGH);  // flash the onboard LED to confirm receipt
  delay(150);
  digitalWrite(LED_BUILTIN, LOW);
}

// ---------------- Main ----------------
void setup() {
  pinMode(SPEAKER_OUT, OUTPUT);
  pinMode(BUTTON_IN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);  // must match BAUD_RATE in DiceLogger.py

  display.setBrightness(brightness);
  display.clear();

  // Start the debounce from the pin's real state so a pin that reads LOW at
  // boot isn't mistaken for a press.
  lastReading = buttonHandled = (digitalRead(BUTTON_IN) == LOW);
  lastChangeTime = millis();

  modeDisplayUntil = millis() + MODE_DISPLAY_MS;  // show the starting mode briefly
  randomSeed(analogRead(A1));                     // unconnected pin: noise gives a different seed each boot
}

void loop() {
  checkForIncomingTime();

  if (buttonPressed()) {
    Serial.println("Roll button pressed - rolling");
    stampActive = false;  // a new roll cancels any timestamp still showing
    spinAnimation();
    rollDice();
  }

  updateSoundModeFromPot();
  updateDisplay();
}
