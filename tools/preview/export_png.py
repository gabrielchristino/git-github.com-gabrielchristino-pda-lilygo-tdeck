"""Export the renderer's RGB framebuffer captures with Python's standard library."""
import argparse
from pathlib import Path
import struct
import zlib


def chunk(kind, data):
    return (
        struct.pack('>I', len(data)) + kind + data
        + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)
    )


def export(source, destination):
    with source.open('rb') as stream:
        if stream.readline() != b'P6\n':
            raise ValueError('Expected a binary RGB PPM capture')
        width, height = map(int, stream.readline().split())
        if stream.readline() != b'255\n':
            raise ValueError('Expected 8-bit color channels')
        pixels = stream.read()
    stride = width * 3
    if len(pixels) != stride * height:
        raise ValueError('Incomplete framebuffer capture')
    rows = b''.join(b'\0' + pixels[y * stride:(y + 1) * stride] for y in range(height))
    data = b'\x89PNG\r\n\x1a\n'
    data += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(rows))
    data += chunk(b'IEND', b'')
    destination.write_bytes(data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    for capture in sorted(args.source.glob('*.ppm')):
        export(capture, args.destination / (capture.stem + '.png'))
        print(capture.stem + '.png')
