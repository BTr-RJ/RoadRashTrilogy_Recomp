"""Repeatable native boot/menu/race probe, with screenshots and retained logs."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("games", nargs="*", default=["rr1", "rr2", "rr3"])
parser.add_argument("--interpreter", action="store_true")
parser.add_argument("--frames", type=int, default=2800)
args = parser.parse_args()

for game in args.games:
    out = ROOT / "build/validation" / game / ("interpreter" if args.interpreter else "native")
    out.mkdir(parents=True, exist_ok=True)
    binary_dir = ROOT / "build/native/bin" / game
    lines = ["WAIT 900", "SCREENSHOT probe_title.png"]
    for i in range(1, 5):
        lines += ["PRESS START 2", "WAIT 240", f"SCREENSHOT probe_menu{i}.png"]
    lines += ["HOLD B", "WAIT 600", "SCREENSHOT probe_race.png", "DUMP_RAM probe_ram.bin"]
    script = out / "progress.input"
    script.write_text("\n".join(lines) + "\n")
    command = [str(binary_dir / f"{game}.exe"), str(ROOT / f".local/build-inputs/{game}/game.bin"),
               "--benchmark", str(args.frames), "--pacing", "accurate", "--input-script", str(script),
               "--hash-frames", "300", "--exec-coverage-out", str(out / "interpreter-pcs.txt")]
    env = os.environ.copy()
    if args.interpreter:
        env["GENESIS_FORCE_INTERP"] = "1"
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    started = time.monotonic()
    with (out / "run.log").open("w") as log:
        try:
            result = subprocess.run(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=90, startupinfo=startup)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    for path in binary_dir.glob("probe_*"):
        if path.suffix in (".png", ".bin"):
            shutil.copyfile(path, out / path.name)
    log_text = (out / "run.log").read_text(errors="replace")
    summary = {"game": game, "returncode": code, "seconds": round(time.monotonic() - started, 2),
               "frames_requested": args.frames, "interpreter_forced": args.interpreter,
               "crash": "CRASH REPORT" in log_text, "output": str(out)}
    (out / "result.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary), flush=True)
    print(log_text[-1800:], flush=True)
