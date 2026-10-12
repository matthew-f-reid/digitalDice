"""
Reads dice rolls from the Arduino Nano over USB serial and saves each one
into MongoDB.

Install:  pip install pyserial pymongo
Run:      python DiceLogger.py

Notes:
  - Close the Arduino IDE Serial Monitor first - only one program can use
    the COM port at a time. Also stop this script before uploading a new
    sketch, for the same reason.
  - MongoDB must already be running on this PC.
"""
import sys
import time
from datetime import datetime

import serial
from pymongo import MongoClient
from pymongo.errors import PyMongoError

SERIAL_PORT = "COM6"  # check Arduino IDE > Tools > Port
BAUD_RATE = 9600      # must match Serial.begin() in the sketch
MONGO_URI = "mongodb://localhost:27017"
DB_NAME = "digitalDice"
COLLECTION_NAME = "rolls"

# Mode number -> name, matching the sound list in the sketch. Only used to
# store a readable name next to the number; update it if you change the modes.
SOUND_NAMES = [
    "Silent", "Ascending sweep", "Dice rattle", "Descending arcade sweep",
    "Alternating two-tone clack", "Smooth rising siren", "Chiptune arpeggio",
    "Laser zap", "Bell / chime", "Robot beep-boop", "Casino jackpot trill",
    "Wind-up spring", "Xylophone", "Sci-fi power-up",
    "Typewriter clack with end ding", "Cymbal hiss / shimmer",
]


def parseRoll(line):
    """Turn 'ROLL,3,5,7' into (3, 5, 7): die 1, die 2, sound mode.
    An older sketch that sends only 'ROLL,3,5' still works (mode is None).
    Returns None for any other line."""
    if not line.startswith("ROLL,"):
        return None
    parts = line.split(",")
    try:
        die1, die2 = int(parts[1]), int(parts[2])
        mode = int(parts[3]) if len(parts) > 3 else None
    except (ValueError, IndexError):
        return None
    if not (1 <= die1 <= 6 and 1 <= die2 <= 6):
        return None
    return die1, die2, mode


def parseSoundMode(line):
    """Turn 'Sound mode -> 7' into 7. Returns None for any other line."""
    if not line.startswith("Sound mode -> "):
        return None
    try:
        return int(line.split("->")[1])
    except (ValueError, IndexError):
        return None


def main():
    client = MongoClient(MONGO_URI, serverSelectionTimeoutMS=3000)
    try:
        client.admin.command("ping")
    except PyMongoError as err:
        sys.exit(f"Can't reach MongoDB at {MONGO_URI}: {err}")
    rolls = client[DB_NAME][COLLECTION_NAME]

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException as err:
        sys.exit(f"Can't open {SERIAL_PORT} (is Serial Monitor still open?): {err}")

    time.sleep(2)  # opening the port resets the Nano; give it time to boot
    print(f"Listening on {SERIAL_PORT} -> {DB_NAME}.{COLLECTION_NAME}. Ctrl+C to stop.")

    lastAnnouncedMode = None  # latest mode seen in a "Sound mode -> N" line

    while True:
        raw = ser.readline().decode("utf-8", errors="ignore").strip()
        announced = parseSoundMode(raw)
        if announced is not None:
            lastAnnouncedMode = announced
        """print("")"""
        roll = parseRoll(raw)
        if roll is None:
            if raw:
                print(f"Sent to Arduino: {raw}")  # debug lines from the sketch
            continue

        die1, die2, mode = roll
        if mode is None:
            # Older sketch: ROLL line has no mode, so use the last one it announced
            mode = lastAnnouncedMode
        if mode is None:
            soundName = None
        elif 0 <= mode < len(SOUND_NAMES):
            soundName = SOUND_NAMES[mode]
        else:
            soundName = "Unknown"
        try:
            rolledAt = datetime.now()  # local time on this PC (no timezone attached)
            doc = {
                "die1": die1,
                "die2": die2,
                "total": die1 + die2,
                "rolledAt": rolledAt,
            }
            if mode is not None:
                doc["soundMode"] = mode
                doc["soundName"] = soundName
            rolls.insert_one(doc)
            soundNote = (f" (sound {mode}: {soundName})" if mode is not None
                          else " (sound mode unknown - upload the updated sketch)")
            print(f"Recieved on Python: Saved roll: {die1} + {die2} = {die1 + die2}{soundNote}")
            
            cleanTimeString = rolledAt.strftime('%Y-%m-%d %H:%M:%S')
    
            serialMessage = f"{cleanTimeString}\n"
            ser.write(serialMessage.encode('utf-8'))
            ser.flush()
        except PyMongoError as err:
            print(f"Failed to save roll {die1},{die2}: {err}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")
