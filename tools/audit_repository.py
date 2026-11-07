"""Check that a public checkout or source package contains only supported files."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('package_release',ROOT/'tools/package_release.py')
release=importlib.util.module_from_spec(spec);spec.loader.exec_module(release)


def audit(root,package=False):
    root=Path(root).resolve()
    if package:
        files=[p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()]
    else:
        raw=subprocess.check_output(['git','-C',str(root),'ls-files','-z'])
        files=raw.decode('utf-8').strip('\0').split('\0')
    unexpected=[name for name in files if name!='PACKAGE-MANIFEST.json' and not release.is_public_path(name)]
    if unexpected:raise ValueError('Non-public files: '+', '.join(unexpected))
    missing=[name for name in ('CMakeLists.txt','README.md','scripts/install.ps1',
             'scripts/build.ps1','tools/public-tools.lock.json','patches/runtime-integration.patch',
             'games/supported-roms.json','src/game/game_spec.c','src/launcher/main.cpp') if not (root/name).is_file()]
    if missing:raise ValueError('Required source files missing: '+', '.join(missing))
    print(f'Public source audit passed: {len(files)} files; build, installation and license inputs retained.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--package',action='store_true',help='Inspect a source package without Git metadata')
    args=parser.parse_args()
    audit(args.root,args.package)
