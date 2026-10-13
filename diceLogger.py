"""
Reads dice rolls from the Arduino Nano over USB serial, saves each one into
MongoDB, and sends the roll's local time back to the Nano.

Install:  pip install pyserial pymongo
Run:      python diceLogger.py

Close the Arduino Serial Monitor first (only one program can use the COM port),
and stop this script before uploading a new sketch. MongoDB must be running.
"""
import re
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

# Index = sound mode number, matching the sound list in the sketch.
SOUND_NAMES = [
    "Silent", "Ascending sweep", "Dice rattle", "Descending arcade sweep",
    "Alternating two-tone clack", "Smooth rising siren", "Chiptune arpeggio",
    "Laser zap", "Bell / chime", "Robot beep-boop", "Casino jackpot trill",
    "Wind-up spring", "Xylophone", "Sci-fi power-up",
    "Typewriter clack with end ding", "Cymbal hiss / shimmer",
]

# "ROLL,3,5,7" = die 1, die 2, sound mode. The mode is optional so a roll
# from an older sketch ("ROLL,3,5") is still saved, just without sound fields.
ROLL_PATTERN = re.compile(r"ROLL,([1-6]),([1-6])(?:,(\d+))?")


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

    while True:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        match = ROLL_PATTERN.fullmatch(line)
        if not match:
            if line:
                print(f"Sent to Arduino: {line}")  # other lines from the sketch
            continue

        die1, die2 = int(match[1]), int(match[2])
        rolledAt = datetime.now()  # local time on this PC
        doc = {"die1": die1, "die2": die2, "total": die1 + die2, "rolledAt": rolledAt}

        soundNote = ""
        if match[3] is not None:
            mode = int(match[3])
            soundName = SOUND_NAMES[mode] if mode < len(SOUND_NAMES) else "Unknown"
            doc.update(soundMode=mode, soundName=soundName)
            soundNote = f" (sound {mode}: {soundName})"

        try:
            rolls.insert_one(doc)
        except PyMongoError as err:
            print(f"Failed to save roll {die1},{die2}: {err}")
            continue

        print(f"Recieved on Python: Saved roll: {die1} + {die2} = {die1 + die2}{soundNote}")
        ser.write(f"{rolledAt:%Y-%m-%d %H:%M:%S}\n".encode())  # the Nano shows this on its display


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")
