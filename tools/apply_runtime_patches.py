"""Apply the complete, pinned runtime changes without spurious Ninja rebuilds."""
from pathlib import Path
import difflib,os,runpy,zipfile
ROOT=Path(__file__).resolve().parents[1]
names=['runner/glue.c','runner/sim_step.c','runner/main.c','runner/video/genesis_vdp.c','runner/video/genesis_vdp.h','runner/video/genesis_machine.c','runner/input_map.h','runner/input_map.c','runner/app_config.c','runner/gamepad.c','runner/gamepad.h']
names.append('recompiler/src/game_config.c')
names.append('external/m68k-recomp-core/profiles/genesis/code_generator.c')
names.append('runner/host_state.h')
names.append('runner/m68k_interp.c')
before={}
for name in names:
    p=ROOT/'.tools/engine'/name
    if p.exists():before[name]=(p.read_bytes(),p.stat())
for script in ['patch_engine.py','patch_raster.py','patch_raster_timing.py','patch_sprite_geometry.py','patch_presentation.py','patch_controls.py','patch_codegen.py','patch_campaign_states.py']:
    runpy.run_path(str(ROOT/'tools'/script),run_name='__main__')
patch=[]
with zipfile.ZipFile(ROOT/'.tools/downloads/segagenesisrecomp-252535d.zip') as z:
    for name in names:
        p=ROOT/'.tools/engine'/name
        if name in before and p.read_bytes()==before[name][0]:
            stat=before[name][1];os.utime(p,ns=(stat.st_atime_ns,stat.st_mtime_ns))
        if name.startswith('external/m68k-recomp-core/'):
            with zipfile.ZipFile(ROOT/'.tools/downloads/m68k-core-e9ebf608933969903edd80ac0b79d41ddb2f013e.zip') as core:
                source=next(n for n in core.namelist() if n.endswith('/'+name.removeprefix('external/m68k-recomp-core/')))
                original=core.read(source).decode().replace('\r\n','\n')
        else:
            source=next(n for n in z.namelist() if n.endswith('/'+name))
            original=z.read(source).decode().replace('\r\n','\n')
        patch.extend(difflib.unified_diff(original.splitlines(True),p.read_text(encoding='utf-8').splitlines(True),fromfile='a/'+name,tofile='b/'+name))
(ROOT/'patches/runtime-integration.patch').write_text(''.join(patch),encoding='utf-8')
