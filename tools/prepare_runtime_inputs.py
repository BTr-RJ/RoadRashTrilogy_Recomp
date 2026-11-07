"""Stage exact imported ROM revisions and configs for the native build."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]

for game in ("rr1", "rr2", "rr3"):
    manifest=ROOT / '.local/games' / f'{game}.json'
    if not manifest.exists():
        print(game, 'not installed; skipping')
        continue
    state = json.loads(manifest.read_text())
    source = ROOT / state["rom"]
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != state["report"]["sha256"]:
        raise ValueError(f"ROM hash changed: {game}")
    stage = ROOT / ".local/build-inputs" / game
    stage.mkdir(parents=True, exist_ok=True)
    target = stage / "game.bin"
    if not target.exists() or target.read_bytes() != data:
        target.write_bytes(data)
    config = (ROOT / state["config"]).read_text()
    # Resolve discovery in the build-owned folder rather than the import folder.
    import re
    config = re.sub(r"discovery_files = \[.*?\]", 'discovery_files = ["discovery.toml"]', config)
    if (stage / "runtime-observed.toml").exists():
        config = config.replace('discovery_files = ["discovery.toml"]',
                                'discovery_files = ["discovery.toml", "runtime-observed.toml"]')
    config = config.replace("[ram_layout]\n", "[ram_layout]\nvbla_stack = 0xFFFFF6\nintr_stack = 0xFFFFF6\n")
    # Verified music-start entry stores D0 into the 68K sound-driver queue.
    hooks={"rr1":[0x1A122,0x1A1FA],"rr2":[0x2BE64,0x2BF3C],"rr3":[0x3A9DE,0x3AADE]}[game]
    # Native actor projection, after descriptor construction and before cull.
    # move.w (a0),d0; cmp.w #-32,d0, verified in each hash-bound local ROM.
    projection={"rr1":0xDC58,"rr2":0x12642,"rr3":0x17DF6}[game]
    if data[projection:projection+6]!=bytes.fromhex('3010b07cffe0'):
        raise ValueError(f"Actor projection opcode mismatch: {game}")
    hooks.append(projection)
    # Read-only object-to-SAT correspondence at the software scaler boundary.
    boundaries={'rr1':[0x9484,0x9476],
                'rr2':[0xA5C8,0xA5B8,0x26EDA],
                'rr3':[0x1C24,0x29160,0x29B16]}[game]
    expected=['33fc000000ff','4cdf0300d1fc','205f301f321f']
    for pc, opcode in zip(boundaries, expected):
        if data[pc:pc+6] != bytes.fromhex(opcode):
            raise ValueError(f'Sprite scaler opcode mismatch: {game} ${pc:X}')
    hooks.extend(boundaries)
    project,opcode={'rr1':(0xFE3A,'48e70ffe3800'),'rr2':(0x1612A,'48e70ffe3800'),
                    'rr3':(0x1BDB8,'48e707fe3800')}[game]
    if data[project:project+6]!=bytes.fromhex(opcode):
        raise ValueError(f'World projection opcode mismatch: {game}')
    hooks.append(project)
    scenery,opcode={'rr1':(0x10C7E,'320006400100'),'rr2':(0x171C8,'320033c000ff'),
                   'rr3':(0x1CB96,'320033c000ff')}[game]
    if data[scenery:scenery+6]!=bytes.fromhex(opcode):
        raise ValueError(f'Scenery projection opcode mismatch: {game}')
    hooks.append(scenery)
    actor_hooks={'rr1':[(0xF44C,'33fc000000ff1b16'),(0xF4CA,'0c79000100ff3040')],
                 'rr2':[(0x156B6,'33fc000000ff066a'),(0x15734,'0c79000100ff1bfe')],
                 'rr3':[(0x1AFAC,'217c000000000018'),(0x1B032,'0c79000100ff1bd0')]}[game]
    for pc,opcode in actor_hooks:
        expected=bytes.fromhex(opcode)
        if data[pc:pc+len(expected)]!=expected:raise ValueError(f'Actor scope opcode mismatch: {game} ${pc:X}')
        hooks.append(pc)
    if game == 'rr3':
        # Before the native race loader decides whether to allocate motors.
        if data[0x10B14:0x10B1A] != bytes.fromhex('4a7900ff752c'):
            raise ValueError('RR3 motor-option opcode mismatch')
        hooks.append(0x10B14)
    for pc in hooks:
        config+=f'\n[[widescreen_site]]\naddr = 0x{pc:X}\nkind = "game_hook"\n'
    profile = stage / "game.toml"
    if not profile.exists() or profile.read_text() != config:
        profile.write_text(config)
    discovery = stage / "discovery.toml"
    reference = ROOT / "games" / game / "discovery.toml"
    if not discovery.exists() or discovery.read_bytes() != reference.read_bytes():
        shutil.copyfile(reference, discovery)
    print(game, state["report"]["sha256"], stage)
