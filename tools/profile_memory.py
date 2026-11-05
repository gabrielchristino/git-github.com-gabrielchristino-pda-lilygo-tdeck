"""Measure PDA heaps and exercise a bounded PSRAM reserve over USB, without editing documents."""
import argparse
import json
from pathlib import Path
import re
import time
import serial
from serial.tools import list_ports

APPS = [('agenda', 0), ('calend', 1), ('notas', 2), ('desenho', 3), ('rel', 4),
        ('contatos', 5), ('calcul', 6), ('planilha', 7), ('diag', 8), ('ajustes', 9),
        ('jogos', 10), ('mapa', 11), ('bluetooth', 12)]


def fields(line):
    return {key: int(value) if value.isdigit() else value
            for key, value in re.findall(r'(\w+)=([^\s]+)', line)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reserve-mib', type=float, nargs='*', default=[2.5, 3.5, 4.5])
    parser.add_argument('--map-stress', action='store_true', help='Also exercise both zoom limits and an address search')
    args = parser.parse_args()
    if any(not 0 < value <= 5 for value in args.reserve_mib):
        parser.error('Reserve sizes must be greater than zero and at most 5 MiB')
    if not args.port:
        matches = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
        if len(matches) != 1:
            parser.error('Specify --port: exactly one matching device is required')
        args.port = matches[0]
    args.output.mkdir(parents=True, exist_ok=False)
    records = []
    log = (args.output / 'serial.txt').open('w', encoding='utf-8')
    port = serial.Serial()
    port.port, port.baudrate, port.timeout, port.write_timeout = args.port, 115200, .05, 2
    port.dtr, port.rts = True, False
    port.open()
    search = None

    def command(text, expected, timeout=8):
        log.write('> ' + text + '\n'); log.flush()
        started = time.perf_counter()
        port.write((text + '\n').encode('ascii'))
        data = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data.extend(port.read(port.in_waiting or 1))
            response = data.decode('utf-8', errors='replace')
            if any(expected in line and line.endswith('\n') for line in response.splitlines(keepends=True)):
                log.write(response); log.flush()
                return response, round((time.perf_counter() - started) * 1000, 3)
        log.write(data.decode('utf-8', errors='replace')); log.flush()
        raise RuntimeError('Device did not confirm: ' + text)

    def home(clear=True):
        nonlocal search
        command('PDA HOME', 'PDA HOME OK')
        if clear:
            for _ in range(24 if search is None else len(search)):
                command('PDA KEY 8', 'PDA INPUT OK')
            search = ''

    def open_app(query, app):
        nonlocal search
        home()
        command('PDA KEY 4', 'PDA INPUT OK')
        for char in query:
            command('PDA KEY ' + str(ord(char)), 'PDA INPUT OK')
        search = query
        _, latency = command('PDA KEY 13', 'PDA INPUT OK')
        status, _ = command('PDA STATUS', 'PDA READY=')
        state = fields(next(line for line in status.splitlines() if line.startswith('PDA READY=')))
        if state.get('APP') != app or state.get('HOME') != 0:
            raise RuntimeError('App did not open: ' + query)
        return latency

    def memory():
        response, _ = command('PDA MEM', 'MEM DONE')
        return {name: fields(next(line for line in response.splitlines() if line.startswith('MEM ' + name + ' ')))
                for name in ('INTERNAL', 'PSRAM', 'PROBE')}

    def buffer_pass():
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            state = memory()['PROBE']['STATE']
            if state == 'PASS':
                return
            if state not in ('WRITE', 'VERIFY'):
                raise RuntimeError('Buffer test did not pass: ' + str(state))
            time.sleep(.1)
        raise RuntimeError('Buffer integrity test timed out')

    def record(label, reserve, save=False, latency=None):
        time.sleep(.25)
        entry = {'stage': label, 'reserve_bytes': reserve, 'memory': memory(), 'open_ack_ms': latency}
        if save:
            result, _ = command('PDA MEM SAVE', 'MEM DONE')
            saved = fields(next(line for line in result.splitlines() if line.startswith('MEM SAVE ')))
            if saved.get('OK') != 1:
                raise RuntimeError('Saving failed')
            entry['save_us'] = saved['US']
            time.sleep(.12)
            entry['after_save'] = memory()
        records.append(entry)
        (args.output / 'measurements.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
        p = entry['memory']['PSRAM']
        print(f'{label}: reserved={reserve:,} PSRAM free={p["FREE"]:,} largest={p["LARGEST"]:,}', flush=True)

    try:
        time.sleep(.5)
        port.reset_input_buffer()
        before_games, _ = command('PDA GAMES', 'GAME I=9')
        command('PDA MEM RELEASE', 'MEM RELEASE OK')
        command('PDA MEM RESERVE 0', 'MEM RESERVE ERROR')
        command('PDA MEM RESERVE 5242884', 'MEM RESERVE ERROR')
        command('PDA MEM VERIFY', 'MEM VERIFY ERROR')
        for mib in [0, *args.reserve_mib]:
            reserved = int(mib * 1024 * 1024) // 4 * 4
            command('PDA MEM RELEASE', 'MEM RELEASE OK')
            home()
            if reserved:
                command('PDA MEM RESERVE ' + str(reserved), 'MEM RESERVE OK')
                command('PDA MEM RESERVE 4', 'MEM RESERVE ERROR')
                buffer_pass()
            command('PDA MEM RESET', 'MEM RESET OK')
            record('home', reserved, save=True)
            for query, app in APPS:
                latency = open_app(query, app)
                if app == 11:
                    for _ in range(4):
                        command('PDA MOVE 1 0', 'PDA INPUT OK')
                    command('PDA MAP', 'MAP VECTOR')
                record(query, reserved, save=app in (11, 12), latency=latency)
                if app == 11 and args.map_stress:
                    command('PDA KEY 99', 'PDA INPUT OK')
                    for key, label in [(111, 'map-wide'), (105, 'map-close')]:
                        for _ in range(8):
                            command('PDA KEY ' + str(key), 'PDA INPUT OK', timeout=20)
                        time.sleep(.5)
                        record(label, reserved, save=True)
                    command('PDA KEY 98', 'PDA INPUT OK')
                    for char in 'hebe camargo, 100':
                        command('PDA KEY ' + str(ord(char)), 'PDA INPUT OK')
                    _, search_ms = command('PDA KEY 13', 'PDA INPUT OK', timeout=20)
                    record('map-search', reserved, latency=search_ms)
                    # Home closes the search through the normal UI path before the next app.
            if reserved:
                command('PDA MEM VERIFY', 'MEM VERIFY START')
                buffer_pass()
            command('PDA MEM CHECK', 'MEM CHECK OK=1')
            command('PDA MEM RELEASE', 'MEM RELEASE OK')
            home()
            record('released', 0)
        after_games, _ = command('PDA GAMES', 'GAME I=9')
        select_games = lambda text: [line for line in text.splitlines() if line.startswith('GAME I=')]
        if select_games(before_games) != select_games(after_games) or len(select_games(after_games)) != 10:
            raise RuntimeError('Game state changed during the profile')
        status, _ = command('PDA STATUS', 'PDA READY=')
        (args.output / 'status-after.txt').write_text(status, encoding='utf-8')
        print('Profile passed; buffers released, heaps valid and game reports unchanged.', flush=True)
    finally:
        try:
            command('PDA MEM RELEASE', 'MEM RELEASE OK', timeout=2)
            home()
        finally:
            port.close(); log.close()


if __name__ == '__main__':
    main()
