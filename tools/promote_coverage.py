"""Stage observed entry candidates locally, bound to the exact imported ROM.

This is discovery evidence, not proof of complete code coverage or correctness.
"""
from pathlib import Path
import hashlib
import json
import re
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]
for game in sys.argv[1:] or ["rr1", "rr2", "rr3"]:
    state = json.loads((ROOT / ".local/games" / f"{game}.json").read_text())
    stage = ROOT / ".local/build-inputs" / game
    data = (stage / "game.bin").read_bytes()
    if hashlib.sha256(data).hexdigest() != state["report"]["sha256"]:
        raise ValueError("ROM identity differs from import")
    entries = set()
    existing = stage / "runtime-observed.toml"
    if existing.exists():
        entries.update(tomllib.loads(existing.read_text()).get("functions", {}).get("extra", []))
    evidence = ROOT / "build/native/bin" / game / "floor_coverage.toml"
    coverage = tomllib.loads(evidence.read_text())
    entries.update(coverage.get("functions", {}).get("extra", []))
    if any(a % 2 or not 0x200 <= a < len(data) for a in entries):
        raise ValueError("Invalid observed entry")
    text = "# Experimental runtime-observed candidates; retained locally.\n"
    text += f"# ROM SHA256: {state['report']['sha256']}\n"
    text += f"# Evidence SHA256: {hashlib.sha256(evidence.read_bytes()).hexdigest()}\n"
    text += "[functions]\nextra = [\n" + "".join(f"  0x{a:06X},\n" for a in sorted(entries)) + "]\n"
    existing.write_text(text)
    print(game, len(entries), "observed candidates staged")
