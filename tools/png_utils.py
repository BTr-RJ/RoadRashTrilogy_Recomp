"""Small lossless PNG helpers for engine-generated QA frames (RGB/RGBA8)."""
import struct
import zlib
from pathlib import Path


def read(path):
    data = Path(path).read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Expected PNG')
    offset, compressed = 8, bytearray()
    while offset < len(data):
        count = struct.unpack_from('>I', data, offset)[0]
        kind, block = data[offset+4:offset+8], data[offset+8:offset+8+count]
        offset += count + 12
        if kind == b'IHDR':
            width, height, depth, color, *_ = struct.unpack('>IIBBBBB', block)
        if kind == b'IDAT':
            compressed.extend(block)
    if depth != 8 or color not in (2, 6):
        raise ValueError('Expected RGB/RGBA8 PNG')
    channels = 4 if color == 6 else 3
    stride, rows = width * channels, []
    raw, previous = zlib.decompress(compressed), bytearray(width * channels)
    def paeth(a, b, c):
        p = a+b-c
        return min((a, b, c), key=lambda value: abs(p-value))
    for y in range(height):
        at = y * (stride+1)
        method, row = raw[at], bytearray(raw[at+1:at+stride+1])
        if method:
            for x in range(stride):
                a = row[x-channels] if x >= channels else 0
                b, c = previous[x], previous[x-channels] if x >= channels else 0
                row[x] = (row[x] + (0, a, b, (a+b)//2, paeth(a, b, c))[method]) & 255
        rows.append(bytes(row)); previous = row
    return width, height, channels, rows


def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind+payload))


def write(path, width, height, rows, channels=3):
    header = struct.pack('>IIBBBBB', width, height, 8, 6 if channels == 4 else 2, 0, 0, 0)
    raw = b''.join(b'\0'+bytes(row) for row in rows)
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', header)+
                          chunk(b'IDAT', zlib.compress(raw, 6))+chunk(b'IEND', b''))
