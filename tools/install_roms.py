"""Find supported, user-supplied cartridge images and stage private build inputs."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MAX_ROM = 4*1024*1024

def candidates(paths):
    seen = set()
    for supplied in paths:
        supplied = Path(supplied)
        if not supplied.exists():
            raise ValueError(f'ROM folder or file not found: {supplied}')
        paths = [supplied] if supplied.is_file() else supplied.rglob('*')
        scanned = 0
        for path in paths:
            if not path.is_file() or path.is_symlink() or path in seen:
                continue
            seen.add(path)
            suffix = path.suffix.lower()
            if suffix not in ('.bin','.md','.gen','.zip'):
                continue
            scanned += 1
            if scanned > 4096:
                raise ValueError('Choose a smaller ROM folder (over 4096 candidates)')
            if suffix == '.zip':
                if path.stat().st_size > 128*1024*1024:
                    continue
                try:
                    with zipfile.ZipFile(path) as archive:
                        for item in archive.infolist():
                            if Path(item.filename).suffix.lower() in ('.bin','.md','.gen') and 512 <= item.file_size <= MAX_ROM:
                                yield path,archive.read(item)
                except (zipfile.BadZipFile, RuntimeError):
                    continue
            elif 512 <= path.stat().st_size <= MAX_ROM:
                yield path,path.read_bytes()

def stage(root, paths):
    supported = json.loads((root/'games/supported-roms.json').read_text())
    by_hash = {entry['sha256']: game for game,entry in supported.items()}
    found = {}
    for path,data in candidates(paths):
        digest = hashlib.sha256(data).hexdigest()
        if digest in by_hash:
            found.setdefault(by_hash[digest],(path,data,digest))
    if not found:
        raise ValueError('No supported ROM revisions found. Use the USA/Europe dumps listed in docs/INSTALLATION.md; Road Rash II must be RR206.')
    for game,(source,data,digest) in found.items():
        target = root/'.local/roms'/game/(digest+'.bin')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise ValueError(f'Failed to verify private ROM copy: {game}')
        profile = root/'.local/profiles'/game/digest/'game.toml'
        profile.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(root/'games'/game/'runtime.toml',profile)
        build = root/'.local/build-inputs'/game
        build.mkdir(parents=True,exist_ok=True)
        shutil.copy2(root/'games'/game/'runtime-observed.toml',build)
        report = dict(supported[game],overseas_title=supported[game]['title'])
        state = dict(schema=1,game=game,rom=target.relative_to(root).as_posix(),
                     config=profile.relative_to(root).as_posix(),qualified=True,report=report)
        manifest = root/'.local/games'/f'{game}.json'
        manifest.parent.mkdir(parents=True,exist_ok=True)
        manifest.write_text(json.dumps(state,indent=2)+'\n')
        print(f'[ROM] {supported[game]["title"]}: verified and staged ({digest})',flush=True)
    return sorted(found)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom-dir',action='append',required=True)
    args = parser.parse_args()
    try:
        stage(ROOT,args.rom_dir)
    except (ValueError,OSError,zipfile.BadZipFile) as error:
        raise SystemExit(str(error))
