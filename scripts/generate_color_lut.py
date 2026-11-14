#!/usr/bin/env python3
"""Generate Piccolo's horizontal blue-slice LUT as an RGB PNG using the standard library."""

import argparse
from pathlib import Path
import struct
import zlib


def grade_color(red, green, blue, style):
    """Apply a bounded color transform to normalized sRGB components."""
    if style == "identity":
        return red, green, blue

    # Mix a small amount of green into red and reduce blue for a warm palette.
    # All coefficients are nonnegative, and each row sums to at most one.
    return 0.96 * red + 0.04 * green, 0.98 * green + 0.02 * red, 0.90 * blue + 0.03 * green


def make_lut(size, style):
    """Return row-major pixels: x = blue * size + red, y = green."""
    if not 2 <= size <= 64:
        raise ValueError("LUT size must be between 2 and 64.")
    if style not in ("identity", "warm"):
        raise ValueError("LUT style must be identity or warm.")

    pixels = bytearray()
    for green in range(size):
        for blue in range(size):
            for red in range(size):
                color = grade_color(red / (size - 1), green / (size - 1), blue / (size - 1), style)
                pixels.extend(round(component * 255) for component in color)
    return bytes(pixels)


def write_png(path, width, height, pixels):
    """Write unfiltered RGB rows and PNG checksums without external image packages."""
    def chunk(kind, data):
        checksum = zlib.crc32(kind + data)
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)

    stride = width * 3
    scanlines = b"".join(b"\x00" + pixels[row * stride:(row + 1) * stride] for row in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(scanlines))
        + chunk(b"IEND", b"")
    )


def main():
    """Parse the LUT size, palette, and destination from the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", type=int, default=16)
    parser.add_argument("--style", choices=("identity", "warm"), default="warm")
    args = parser.parse_args()
    pixels = make_lut(args.size, args.style)
    write_png(args.output, args.size * args.size, args.size, pixels)


if __name__ == "__main__":
    main()
