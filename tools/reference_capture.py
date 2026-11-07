"""Developer-only Libretro reference capture. NEVER used to launch the port.

Loads the separately installed Genesis Plus GX test oracle from .tools/reference.
"""
import ctypes as C
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
game = sys.argv[1] if len(sys.argv) > 1 else "rr1"
out = ROOT / "build/validation" / game / "gpgx-reference"
out.mkdir(parents=True, exist_ok=True)
core = C.CDLL(str(ROOT / ".tools/reference/genesis_plus_gx_libretro.dll"))
frame = 0
pixel = 0
directory = str(ROOT / ".tools/reference").encode()


def png(path, width, height, rgb):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + rgb[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


@C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
def environment(command, data):
    global pixel
    if command in (9, 31):
        C.cast(data, C.POINTER(C.c_char_p))[0] = directory
        return True
    if command == 10:
        pixel = C.cast(data, C.POINTER(C.c_int))[0]
        return True
    if command == 3:
        C.cast(data, C.POINTER(C.c_bool))[0] = True
        return True
    if command == 17:
        C.cast(data, C.POINTER(C.c_bool))[0] = False
        return True
    if command == 18:
        return True
    return False


@C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
def video(data, width, height, pitch):
    if not data or frame not in (900, 1140, 1800, 2340, 3540):
        return
    pixels = C.string_at(data, pitch * height)
    rgb = bytearray()
    for y in range(height):
        for x in range(width):
            if pixel == 1:
                b, g, r, _ = pixels[y*pitch+x*4:y*pitch+x*4+4]
            else:
                value = struct.unpack_from("<H", pixels, y*pitch+x*2)[0]
                if pixel == 2:
                    r, g, b = (value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31
                else:
                    r, g, b = ((value >> 10) & 31) * 255 // 31, ((value >> 5) & 31) * 255 // 31, (value & 31) * 255 // 31
            rgb.extend((r, g, b))
    png(out / f"frame-{frame}.png", width, height, rgb)


@C.CFUNCTYPE(None, C.c_int16, C.c_int16)
def audio_sample(left, right):
    pass


@C.CFUNCTYPE(C.c_size_t, C.POINTER(C.c_int16), C.c_size_t)
def audio_batch(data, frames):
    return frames


@C.CFUNCTYPE(None)
def input_poll():
    pass


@C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)
def input_state(port, device, index, button):
    if port != 0 or device != 1:
        return 0
    if button == 3 and (900 <= frame < 902 or 1140 <= frame < 1142):
        return 1
    return int(button == 0 and frame >= 1740)


class GameInfo(C.Structure):
    _fields_ = [("path", C.c_char_p), ("data", C.c_void_p), ("size", C.c_size_t), ("meta", C.c_char_p)]


for name, callback in (("environment", environment), ("video_refresh", video),
                       ("audio_sample", audio_sample), ("audio_sample_batch", audio_batch),
                       ("input_poll", input_poll), ("input_state", input_state)):
    fn = getattr(core, "retro_set_" + name)
    fn.argtypes = [type(callback)]
    fn(callback)
core.retro_init()
rom = ROOT / ".local/build-inputs" / game / "game.bin"
data = rom.read_bytes()
buffer = C.create_string_buffer(data)
info = GameInfo(str(rom).encode(), C.cast(buffer, C.c_void_p), len(data), None)
core.retro_load_game.argtypes = [C.POINTER(GameInfo)]
core.retro_load_game.restype = C.c_bool
if not core.retro_load_game(C.byref(info)):
    raise RuntimeError("Reference failed to load the ROM")
for frame in range(3800):
    core.retro_run()
    if frame==1800:
        core.retro_get_memory_size.argtypes=[C.c_uint];core.retro_get_memory_size.restype=C.c_size_t
        core.retro_get_memory_data.argtypes=[C.c_uint];core.retro_get_memory_data.restype=C.c_void_p
        for kind,name in [(2,'ram'),(3,'vram')]:
            size=core.retro_get_memory_size(kind);ptr=core.retro_get_memory_data(kind)
            if size and ptr:(out/(name+'-1800.bin')).write_bytes(C.string_at(ptr,size))
core.retro_get_memory_size.argtypes = [C.c_uint]
core.retro_get_memory_size.restype = C.c_size_t
core.retro_get_memory_data.argtypes = [C.c_uint]
core.retro_get_memory_data.restype = C.c_void_p
size = core.retro_get_memory_size(2)
pointer = core.retro_get_memory_data(2)
if size and pointer:
    (out / "ram-raw.bin").write_bytes(C.string_at(pointer, size))
core.retro_unload_game()
core.retro_deinit()
print("Reference frames captured:", out)
