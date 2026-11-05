"""Send a documented PDA diagnostic command over the device's USB serial port."""
import argparse
import time
import serial
from serial.tools import list_ports

COMMANDS = {
    'status': ('PDA STATUS', 'PDA READY='),
    'memory': ('PDA MEM', 'MEM DONE'),
    'save': ('PDA SAVE', 'PDA SAVED'),
    'home': ('PDA HOME', 'PDA HOME OK'),
    'restart': ('PDA RESTART', 'PDA RESTARTING'),
    'ball': ('PDA BALL', 'BALL IRQ'),
    'games': ('PDA GAMES', 'GAME I=0'),
    'gps': ('PDA GPS', 'GPS RX='),
    'map': ('PDA MAP', 'MAP VECTOR'),
    'bt-peers': ('PDA BT PEERS', 'BT PEERS COUNT='),
    'bt-log': ('PDA BT LOG', 'BT LOG COUNT='),
    'bt-manage': ('PDA BT MANAGE', 'BT MANAGE OK'),
    'bt-select': ('PDA BT SELECT {peer}', 'BT SELECT OK'),
}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port')
parser.add_argument('command', choices=[*COMMANDS, 'time'])
parser.add_argument('--epoch', type=int)
parser.add_argument('--peer', help='Saved identity printed by bt-peers; required for bt-select')
parser.add_argument('--seconds', type=float, default=6, help='Observation duration; GPS is queried every 2 seconds')
args = parser.parse_args()
if not 0 < args.seconds <= 600:
    parser.error('--seconds must be between 0 and 600')
if args.command == 'bt-select' and not args.peer:
    parser.error('--peer is required for bt-select; use bt-peers to list saved identities')
if args.peer and (args.command != 'bt-select' or any(c.isspace() for c in args.peer)):
    parser.error('--peer is a single saved identity and is only valid with bt-select')
if args.command == 'time' and args.epoch is None:
    parser.error('--epoch is required for time')
ports = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
if not args.port:
    if len(ports) != 1:
        parser.error('Specify --port: exactly one matching device is required')
    args.port = ports[0]
if args.command == 'time':
    command = 'PDA TIME ' + str(args.epoch)
    expected = 'PDA TIME OK'
else:
    command, expected = COMMANDS[args.command]
    if args.command == 'bt-select':
        command = command.format(peer=args.peer)
port = serial.Serial()
port.port, port.baudrate, port.timeout = args.port, 115200, 0.2
port.dtr, port.rts = True, False
port.open()
try:
    time.sleep(0.3)
    port.write((command + '\n').encode('ascii'))
    port.flush()
    deadline = time.monotonic() + args.seconds
    next_query = time.monotonic() + 2
    data = bytearray()
    while time.monotonic() < deadline:
        if args.command == 'gps' and time.monotonic() >= next_query:
            port.write(b'PDA GPS\n')
            next_query = time.monotonic() + 2
        part = port.read(port.in_waiting or 1)
        if part:
            data.extend(part)
            print(part.decode('utf-8', errors='replace'), end='', flush=True)
    text = data.decode('utf-8', errors='replace')
    if expected not in text:
        raise SystemExit('Device did not confirm the command')
finally:
    port.close()
