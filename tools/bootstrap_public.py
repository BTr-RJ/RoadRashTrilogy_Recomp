"""Install the exact public SDK dependencies, using verified cached archives."""
from pathlib import Path
import hashlib
import json
import shutil
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def extract(archive, destination, strip=False):
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            if item.is_dir():
                continue
            parts = Path(item.filename.replace('\\','/')).parts
            relative = Path(*parts[1:]) if strip else Path(*parts)
            target = (destination/relative).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise ValueError('Unsafe SDK archive path')
            target.parent.mkdir(parents=True, exist_ok=True)
            data = z.read(item)
            if not target.exists() or target.read_bytes() != data:
                target.write_bytes(data)

def main():
    lock = json.loads((ROOT/'tools/public-tools.lock.json').read_text())
    cache = ROOT/'.tools/downloads'
    cache.mkdir(parents=True, exist_ok=True)
    for index,entry in enumerate(lock['downloads']):
        print(f'[SDK {index+1}/{len(lock["downloads"])}] {entry["name"]}', flush=True)
        archive = cache/entry['archive']
        if not archive.exists():
            partial = archive.with_suffix(archive.suffix+'.partial')
            with urllib.request.urlopen(entry['url'], timeout=120) as response, partial.open('wb') as target:
                shutil.copyfileobj(response,target)
            if hashlib.sha256(partial.read_bytes()).hexdigest() != entry['sha256']:
                raise ValueError(f'Download checksum mismatch: {entry["name"]}')
            partial.replace(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError(f'Cached archive checksum mismatch: {entry["name"]}')
        # Python is already running from this directory; never overwrite a
        # loaded executable/DLL. The PowerShell bootstrap verified its archive.
        if entry['name'] == 'python':
            continue
        if entry.get('kind') == 'file':
            target = ROOT/entry['destination']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(archive,target)
        else:
            extract(archive, ROOT/entry['destination'], entry['strip_top'])
    shader = ROOT/'.tools/librashader'
    (shader/'include').mkdir(exist_ok=True)
    shutil.copy2(shader/'librashader.h',shader/'include/librashader.h')
    dx = ROOT/'.tools/dxsdk/build/native/release/bin/x64'
    for name in ('D3DX9_43.dll','D3DCompiler_43.dll'):
        shutil.copy2(dx/name,shader/name)
    print('[SDK] All archives verified and installed', flush=True)

if __name__ == '__main__':
    main()
