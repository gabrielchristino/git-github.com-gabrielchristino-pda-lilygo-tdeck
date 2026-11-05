"""Flash the PDA application; explicitly migrate the legacy OTA layout with backup."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import time
from datetime import datetime
from contextlib import ExitStack
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
FLASH_SIZE = 0x1000000
TABLE_OFFSET = 0x8000
APP_OFFSET = 0x10000
DATA_OFFSET = 0xC90000
# type, subtype, offset, size, label, flags
NVS = (1, 2, 0x9000, 0x5000, 'nvs', 0)
FILES = (1, 0x82, DATA_OFFSET, 0x360000, 'spiffs', 0)
COREDUMP = (1, 3, 0xFF0000, 0x10000, 'coredump', 0)
LEGACY = [NVS, (1, 0, 0xE000, 0x2000, 'otadata', 0),
          (0, 0x10, APP_OFFSET, 0x640000, 'app0', 0),
          (0, 0x11, 0x650000, 0x640000, 'app1', 0), FILES, COREDUMP]
SINGLE = [NVS, (0, 0, APP_OFFSET, 0xC80000, 'factory', 0), FILES, COREDUMP]


def partitions(payload):
    """Parse the ESP-IDF binary table, requiring its integrity checksum."""
    result = []
    for offset in range(0, min(len(payload), 0xC00), 32):
        row = payload[offset:offset + 32]
        if len(row) != 32:
            break
        if row[:2] == b'\xeb\xeb':
            if row[2:16] != b'\xff' * 14 or row[16:] != hashlib.md5(payload[:offset]).digest():
                raise ValueError('Partition table checksum mismatch')
            return result
        magic, kind, subtype, start, size, name, flags = struct.unpack('<HBBII16sI', row)
        if magic != 0x50AA:
            break
        result.append((kind, subtype, start, size, name.split(b'\0')[0].decode('ascii'), flags))
    raise ValueError('Partition table has no valid MD5 record')


def validate(target, installed, firmware_size, migrate):
    if target != SINGLE:
        raise ValueError('Build does not use the expected single factory layout; rebuild first')
    if not 0 < firmware_size <= SINGLE[1][3]:
        raise ValueError('Firmware size exceeds the application partition or is empty')
    if installed != SINGLE:
        if not migrate:
            raise ValueError('Installed partition table differs. Use -MigratePartitions for the backed-up migration')
        if installed != LEGACY:
            raise ValueError('Unknown installed layout; migration refused without writing')


def save_device(port):
    import serial
    connection = serial.Serial()
    connection.port, connection.baudrate, connection.timeout = port, 115200, 0.2
    connection.write_timeout = 2
    connection.dtr, connection.rts = True, False
    connection.open()
    try:
        time.sleep(0.3)
        connection.reset_input_buffer()
        data = bytearray()
        deadline = time.monotonic() + 12
        next_request = 0
        while time.monotonic() < deadline:
            if time.monotonic() >= next_request:
                connection.write(b'PDA SAVE\n')
                next_request = time.monotonic() + 1
            data.extend(connection.read(connection.in_waiting or 1))
            if b'PDA SAVED' in data:
                return
        raise RuntimeError('PDA did not confirm saving; no flash was written')
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--esptool', type=Path, required=True)
    parser.add_argument('--migrate-partitions', action='store_true')
    parser.add_argument('--environment', choices=['tdeck'], default='tdeck')
    args = parser.parse_args()
    build = ROOT / '.pio/build' / args.environment
    firmware = build / 'firmware.bin'
    table = build / 'partitions.bin'
    target = partitions(table.read_bytes())
    # Reject a stale/wrong build before touching the device.
    validate(target, SINGLE, firmware.stat().st_size, False)
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    directory = ROOT / ('backups' if args.migrate_partitions else 'build') / ('flash-' + stamp)
    directory.mkdir(parents=True, exist_ok=False)
    print('Saving current PDA state...', flush=True)
    save_device(args.port)
    connected = False
    write_started = False
    loader = None
    # Keep one open USB connection and one stub throughout backup/write/verify,
    # avoiding repeated USB enumeration between operations.
    isolated_tools = ROOT / '.pio-tools/flash'
    sys.path.insert(0, str(isolated_tools if (isolated_tools / 'esptool').is_dir()
                           else args.esptool.resolve().parent))
    import esptool
    if args.migrate_partitions and esptool.__version__ != '4.8.1':
        raise RuntimeError('Migration requires the tested esptool 4.8.1: '
                           'python -m pip install --target .pio-tools/flash -r tools/requirements-flash.txt')

    def run(*operation, after='no_reset_stub'):
        nonlocal connected, loader
        if loader is None:
            rom = esptool.get_default_connected_device(
                [args.port], args.port, 3, 115200, chip='esp32s3', before='usb_reset')
            loader = rom.run_stub()
            loader.change_baud(921600)
            loader.flash_set_parameters(FLASH_SIZE)
            connected = True
        name, *values = operation
        if name == 'read_flash':
            start, size, destination = values
            # Bounded reads also provide progress and preserve partial backup files
            # if a cable is disconnected. Only a complete, verified backup is usable.
            with Path(destination).open('wb') as stream:
                for offset in range(0, size, 0x10000):
                    chunk = min(0x10000, size - offset)
                    stream.write(loader.read_flash(start + offset, chunk))
                    stream.flush()
                    if offset == 0 or (offset + chunk) % 0x100000 == 0 or offset + chunk == size:
                        print(f'Read {offset + chunk:,}/{size:,} bytes at {start:#x}', flush=True)
        elif name in ('write_flash', 'verify_flash'):
            if name == 'write_flash':
                # The caller requests compression and preserves image flash settings.
                if values[:7] != ['-z', '--flash_mode', 'keep', '--flash_freq', 'keep', '--flash_size', 'keep']:
                    raise ValueError('Unexpected flash options')
                values = values[7:]
            with ExitStack() as files:
                pairs = [(int(str(values[i]), 0), files.enter_context(Path(values[i + 1]).open('rb')))
                         for i in range(0, len(values), 2)]
                options = SimpleNamespace(addr_filename=pairs, chip='esp32s3',
                    flash_mode='keep', flash_freq='keep', flash_size='keep',
                    compress=True, no_compress=False, no_stub=False, force=False,
                    encrypt=False, encrypt_files=None, erase_all=False, verify=False,
                    ignore_flash_encryption_efuse_setting=False)
                getattr(esptool, name)(loader, options)
        elif name != 'run':
            raise ValueError('Unexpected esptool operation: ' + name)
        if after == 'hard_reset':
            loader.hard_reset()

    try:
        installed_path = directory / 'installed-partitions.bin'
        run('read_flash', TABLE_OFFSET, 0x1000, installed_path)
        installed = partitions(installed_path.read_bytes())
        validate(target, installed, firmware.stat().st_size, args.migrate_partitions)
        protected = []
        if args.migrate_partitions:
            backup = directory / 'flash-before.bin'
            print('Backing up all 16 MiB before migration: ' + str(backup), flush=True)
            run('read_flash', 0, FLASH_SIZE, backup)
            payload = backup.read_bytes()
            if len(payload) != FLASH_SIZE or partitions(payload[TABLE_OFFSET:TABLE_OFFSET + 0x1000]) != installed:
                raise RuntimeError('Incomplete or inconsistent flash backup; migration cancelled')
            run('verify_flash', 0, backup)
            for name, start, end in [('bootloader', 0, TABLE_OFFSET),
                                     ('configuration', 0x9000, APP_OFFSET),
                                     ('documents-coredump', DATA_OFFSET, FLASH_SIZE)]:
                path = directory / (name + '.bin')
                path.write_bytes(payload[start:end])
                protected.extend([hex(start), str(path)])
            (directory / 'manifest.json').write_text(json.dumps({
                'port': args.port, 'flash_bytes': FLASH_SIZE,
                'esptool_version': esptool.__version__,
                'backup_sha256': hashlib.sha256(payload).hexdigest(),
                'firmware_sha256': hashlib.sha256(firmware.read_bytes()).hexdigest(),
                'previous_partitions': installed, 'new_partitions': target,
            }, indent=2) + '\n', encoding='utf-8')
            # Save exact new artifacts beside the recovery image.
            (directory / 'partitions-after.bin').write_bytes(table.read_bytes())
            (directory / 'firmware-after.bin').write_bytes(firmware.read_bytes())
        addresses = []
        if args.migrate_partitions:
            addresses.extend([hex(TABLE_OFFSET), str(table)])
        addresses.extend([hex(APP_OFFSET), str(firmware)])
        write_started = True
        run('write_flash', '-z', '--flash_mode', 'keep', '--flash_freq', 'keep',
            '--flash_size', 'keep', *addresses)
        # Verify the app/table and every protected byte while the PDA is still stopped.
        run('verify_flash', *addresses, *protected, after='hard_reset')
        connected = False
        print('Flash verified. PDA restarted. Records: ' + str(directory), flush=True)
    finally:
        if connected and write_started:
            print('Verification/writing failed. Device left in download mode. Recovery records: '
                  + str(directory), file=sys.stderr)
        elif connected:
            # Validation errors before writing can safely return to the old firmware.
            try:
                run('run', after='hard_reset')
            except (esptool.FatalError, OSError):
                print('Could not reset the device. Recovery records: ' + str(directory), file=sys.stderr)
        if loader is not None:
            loader._port.close()


if __name__ == '__main__':
    main()
