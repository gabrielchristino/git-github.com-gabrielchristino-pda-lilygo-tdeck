"""Safety checks for the no-OTA layout and its migration gates (no serial access)."""
import hashlib
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from flash_device import LEGACY, SINGLE, partitions, validate


def binary(rows):
    data = b''.join(struct.pack('<HBBII16sI', 0x50AA, kind, subtype, offset, size,
                                name.encode('ascii'), flags)
                    for kind, subtype, offset, size, name, flags in rows)
    return data + b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(data).digest()


class LayoutTests(unittest.TestCase):
    def test_legacy_and_single_table_integrity(self):
        for rows in (LEGACY, SINGLE):
            self.assertEqual(partitions(binary(rows)), rows)

    def test_corruption_and_missing_checksum_rejected(self):
        corrupted = bytearray(binary(SINGLE))
        corrupted[8] ^= 1
        for payload in (bytes(corrupted), binary(SINGLE)[:-32], b'\xff' * 4096):
            with self.assertRaises(ValueError):
                partitions(payload)

    def test_only_one_factory_and_preserved_data_regions(self):
        apps = [row for row in SINGLE if row[0] == 0]
        self.assertEqual(len(apps), 1)
        self.assertEqual(apps[0][1:4], (0, 0x10000, 0xC80000))
        self.assertEqual(apps[0][2] + apps[0][3], 0xC90000)
        self.assertFalse(any(row[:2] == (1, 0) for row in SINGLE))
        for row in LEGACY:
            if row[0] == 1 and row[1] != 0:
                self.assertIn(row, SINGLE)
        for first, second in zip(SINGLE, SINGLE[1:]):
            self.assertLessEqual(first[2] + first[3], second[2])
        self.assertEqual(SINGLE[-1][2] + SINGLE[-1][3], 16 * 1024 * 1024)

    def test_migration_requires_explicit_option(self):
        with self.assertRaises(ValueError):
            validate(SINGLE, LEGACY, 5_300_000, False)
        validate(SINGLE, LEGACY, 5_300_000, True)

    def test_unknown_layout_rejected_even_with_migration(self):
        changed = list(LEGACY)
        changed[-2] = (1, 0x82, 0xCA0000, 0x350000, 'spiffs', 0)
        with self.assertRaises(ValueError):
            validate(SINGLE, changed, 5_300_000, True)

    def test_stale_build_and_oversized_firmware_rejected(self):
        for target, size in ((LEGACY, 5_300_000), (SINGLE, 0), (SINGLE, 0xC80001)):
            with self.assertRaises(ValueError):
                validate(target, SINGLE, size, True)

    def test_normal_update_after_migration(self):
        validate(SINGLE, SINGLE, 0xC80000, False)


if __name__ == '__main__':
    unittest.main()
