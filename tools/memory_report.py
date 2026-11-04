"""Measure the built T-Deck ELF using the same sections as PlatformIO.

Run after pio run. This reads build artifacts; it never opens a serial port.
"""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FLASH_SECTIONS = {'.iram0.text', '.iram0.vectors', '.dram0.data', '.flash.text', '.flash.rodata'}
RAM_SECTIONS = {'.dram0.data', '.dram0.bss', '.noinit'}


def tool(name):
    executable = 'xtensa-esp32s3-elf-' + name
    found = shutil.which(executable)
    if found:
        return found
    directory = Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin'
    for suffix in ('.exe', ''):
        candidate = directory/(executable+suffix)
        if candidate.is_file():
            return str(candidate)
    raise SystemExit('Toolchain not found: ' + executable)


def app_partition(path):
    payload = path.read_bytes()
    for offset in range(0, len(payload)-31, 32):
        magic, kind, subtype, _, size, _, _ = struct.unpack_from('<HBBII16sI', payload, offset)
        if magic == 0x50aa and kind == 0 and subtype in (0, 0x10):
            return size
    raise SystemExit('No factory/ota_0 application partition found')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, default=ROOT/'.pio/build/tdeck/firmware.elf')
    parser.add_argument('--baseline-flash', type=int)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    if not args.elf.is_file():
        parser.error('Build the firmware first: pio run -e tdeck')
    section_output = subprocess.check_output([tool('size'), '-A', '-d', str(args.elf)], text=True)
    sections = {}
    for line in section_output.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith('.') and parts[1].isdigit():
            sections[parts[0]] = int(parts[1])
    flash = sum(sections.get(key, 0) for key in FLASH_SECTIONS)
    ram = sum(sections.get(key, 0) for key in RAM_SECTIONS)
    capacity = app_partition(args.elf.with_name('partitions.bin'))
    symbols_output = subprocess.check_output([tool('nm'), '-S', '--size-sort', '--radix=d', '-C', str(args.elf)], text=True)
    symbols = []
    for line in symbols_output.splitlines():
        parts = line.split(None, 3)
        if len(parts) == 4 and parts[1].isdigit() and parts[2] in 'dDrRtT':
            symbols.append({'bytes': int(parts[1]), 'symbol': parts[3]})
    report = {'flash_bytes': flash, 'app_partition_bytes': capacity,
              'flash_percent': round(100*flash/capacity, 2), 'free_app_bytes': capacity-flash,
              'static_ram_bytes': ram, 'largest_symbols': sorted(symbols, key=lambda x: x['bytes'], reverse=True)[:16]}
    if args.baseline_flash is not None:
        report['saved_flash_bytes'] = args.baseline_flash-flash
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return
    print(f"Application: {flash:,} / {capacity:,} bytes ({report['flash_percent']}%); free: {capacity-flash:,}")
    print(f'Static internal RAM: {ram:,} bytes (runtime heap and PSRAM require device measurements)')
    if 'saved_flash_bytes' in report:
        print(f"Saved compared with baseline: {report['saved_flash_bytes']:,} bytes")
    for entry in report['largest_symbols']:
        print(f"{entry['bytes']:>10,}  {entry['symbol']}")


if __name__ == '__main__':
    main()
