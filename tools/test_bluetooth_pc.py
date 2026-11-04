"""Send test input through the T-Deck and verify the local Windows receiver."""
import argparse
import json
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", default="COM4")
parser.add_argument("--directory", type=Path, required=True)
parser.add_argument("--mouse", action="store_true")
parser.add_argument("--wait", type=float, default=0)
args = parser.parse_args()
state_path = args.directory / "state.txt"

def ready(mouse=False):
    state = state_path.read_text()
    return (time.time() - state_path.stat().st_mtime < 2 and
            "ACTIVE=True" in state and "FOCUSED=True" in state and
            (not mouse or "POINTER_INSIDE=True" in state))

deadline = time.monotonic() + min(max(args.wait, 0), 50)
while not ready(args.mouse) and time.monotonic() < deadline:
    time.sleep(0.2)
if not ready(args.mouse):
    raise SystemExit("Receiver must be active and focused; mouse test also needs pointer inside.")
port = serial.Serial(args.port, 115200, timeout=0.2)
try:
    time.sleep(0.3)
    port.write(b"PDA STATUS\n")
    time.sleep(0.3)
    status = port.read(8192).decode(errors="replace")
    if "PDA BLUETOOTH Conectado" not in status:
        raise SystemExit("T-Deck is not connected over Bluetooth.")
    def send(command):
        if not ready(args.mouse):
            raise SystemExit("Receiver lost focus; test stopped.")
        port.write((command + "\n").encode("ascii"))
        time.sleep(0.08)
    if args.mouse:
        before = (args.directory / "events.tsv").read_text()
        # Small balanced motions keep the pointer in the receiver.
        for x, y in [(2, 0), (0, 2), (-2, 0), (0, -2)]:
            send(f"PDA MOVE {x} {y}")
        send("PDA KEY 0")
        time.sleep(0.3)
        events = (args.directory / "events.tsv").read_text()[len(before):]
        assert "\tMOVE\t" in events, events
        assert "\tMOUSE_DOWN\tLeft" in events, events
        assert "\tMOUSE_UP\tLeft" in events, events
        print("BLE mouse movement and left-button press/release received by Windows.")
    else:
        message = "TDeck Bluetooth aAzZ09 !@#$% ()_+-=[]{}<>/?"
        for c in message + "X":
            # Exercise the physical keyboard shortcut for its missing '=' key.
            send(f"PDA KEY {4 if c == '=' else ord(c)}")
        for key in [8, 13, 9, ord("O"), ord("K")]:
            send(f"PDA KEY {key}")
        time.sleep(0.3)
        received = (args.directory / "text.txt").read_text()
        expected = message + "\n\tOK"
        print(json.dumps({"expected": expected, "received": received}, ensure_ascii=False))
        assert received == expected, "Bluetooth keyboard text mismatch"
        print("BLE letters, numbers, symbols, Backspace, Enter and Tab received correctly by Windows.")
finally:
    port.close()
