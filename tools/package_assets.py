#!/usr/bin/env python3
"""Generate PSP disc metadata assets (ICON0.PNG and UMD_DATA.BIN) during CI builds."""

import os
import struct
import sys
import uuid
import zlib

GLYPHS = {
    "R": [0xFC, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0xE6, 0x00],
    "Y": [0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x00],
    "U": [0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00],
    "N": [0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00],
    "A": [0x38, 0x6C, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00],
    "I": [0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00],
    " ": [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
}


def _png_chunk(tag: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(tag + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)


def write_icon0_png(out_path: str) -> None:
    width, height = 144, 80
    buf = bytearray(width * height * 4)

    for y in range(height):
        for x in range(width):
            idx = (y * width + x) * 4
            # Deep twilight gradient background
            r = 16 + (y * 14) // height
            g = 18 + (y * 16) // height
            b = 34 + (y * 28) // height

            # Subtle isometric diamond lattice
            iso_u = (x + y * 2) % 24
            iso_v = (x - y * 2) % 24
            if iso_u == 0 or iso_v == 0:
                r += 10
                g += 12
                b += 20

            # Center isometric emblem behind title
            dx = abs(x - 72)
            dy = abs(y - 26) * 2
            if dx + dy <= 28:
                if dx + dy >= 24:
                    r, g, b = 245, 218, 134
                else:
                    r, g, b = 52, 88, 146

            # Outer gold border
            if x in (0, 1, width - 2, width - 1) or y in (0, 1, height - 2, height - 1):
                r, g, b = 232, 198, 112

            buf[idx : idx + 4] = bytes((min(r, 255), min(g, 255), min(b, 255), 255))

    def put_px(px: int, py: int, col: tuple[int, int, int]) -> None:
        if 0 <= px < width and 0 <= py < height:
            i = (py * width + px) * 4
            buf[i : i + 4] = bytes((col[0], col[1], col[2], 255))

    title = "RYUNA RIA"
    scale = 2
    char_adv = 14
    start_x = (width - len(title) * char_adv) // 2
    start_y = 48

    for ci, ch in enumerate(title):
        rows = GLYPHS.get(ch, GLYPHS[" "])
        gx = start_x + ci * char_adv
        for ry, bits in enumerate(rows):
            for rx in range(8):
                if bits & (0x80 >> rx):
                    for sy in range(scale):
                        for sx in range(scale):
                            put_px(gx + rx * scale + sx + 1, start_y + ry * scale + sy + 1, (10, 12, 22))
                            put_px(gx + rx * scale + sx, start_y + ry * scale + sy, (248, 224, 142))

    raw_scanlines = bytearray()
    for y in range(height):
        raw_scanlines.append(0)  # Filter type 0 (None)
        row_start = y * width * 4
        raw_scanlines.extend(buf[row_start : row_start + width * 4])

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png.extend(_png_chunk(b"IHDR", ihdr))
    png.extend(_png_chunk(b"IDAT", zlib.compress(bytes(raw_scanlines), 9)))
    png.extend(_png_chunk(b"IEND", b""))

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(png)


def write_umd_data_bin(out_path: str, disc_id: str = "RUNA00001") -> None:
    g = uuid.uuid4().bytes_le
    d1 = int.from_bytes(g[0:4], "little")
    d2 = int.from_bytes(g[4:6], "little")
    d3 = int.from_bytes(g[6:8], "little")
    text = f"{disc_id}|{d1:08X}{d2:04X}{d3:04X}|0001|G".encode("ascii")

    data = bytearray(48)
    data[0 : len(text)] = text
    data[47] = ord("|")

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)


def main() -> int:
    if len(sys.argv) < 3:
        print("Usage: package_assets.py <icon0_out.png> <umd_data_out.bin> [DISC_ID]", file=sys.stderr)
        return 1
    disc_id = sys.argv[3] if len(sys.argv) >= 4 else "RUNA00001"
    write_icon0_png(sys.argv[1])
    write_umd_data_bin(sys.argv[2], disc_id)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
