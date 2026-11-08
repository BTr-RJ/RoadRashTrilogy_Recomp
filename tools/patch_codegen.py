"""Keep the pinned layout emitter compatible with C11 (including MSVC 2019)."""
from pathlib import Path


LOOP = '    for (int i = 0; i < L->level_mode_count; i++)\n'
GUARD = ('    if (L->level_mode_count == 0)\n'
         '        fprintf(f, " 0"); /* RR_C11_LAYOUT: no empty array initializer. */\n')


def patch_source(source):
    if source.count(LOOP) != 1:
        raise ValueError('Pinned layout emitter context does not match')
    if GUARD + LOOP in source:
        return source
    if 'RR_C11_LAYOUT' in source:
        raise ValueError('Unexpected existing C11 layout patch')
    return source.replace(LOOP, GUARD + LOOP)


def main():
    root = Path(__file__).resolve().parents[1]
    path = root / '.tools/engine/recompiler/src/game_config.c'
    source = path.read_text(encoding='utf-8')
    patched = patch_source(source)
    if patched != source:
        path.write_text(patched, encoding='utf-8')
    print('Applied C11-compatible layout emission')


if __name__ == '__main__':
    main()
