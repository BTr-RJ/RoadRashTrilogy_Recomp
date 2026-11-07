"""Fetch pinned, project-local runtime sources. No system-wide installation."""
from pathlib import Path
import hashlib
import json
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = {
    "rcheevos": ("RetroAchievements/rcheevos", "eea6461f172d37d36b0d56568f13a505e0a9fd86", ".tools/rcheevos"),
    "m68k-core": ("mstan/m68k-recomp-core", "e9ebf608933969903edd80ac0b79d41ddb2f013e", ".tools/engine/external/m68k-recomp-core"),
    "z80-core": ("mstan/z80-recomp-core", "0e6606a41245d54c5fd0c3dc322ef9d6890d3923", ".tools/engine/external/z80-recomp-core"),
    "rbengine": ("RetroPortingToolKit/rbengine", "2a03e73693acee0fb78076ea058642c931bef12e", ".tools/engine/external/rbengine"),
    "recomp-net": ("RetroPortingToolKit/recomp-net", "2103d5eef111ce98846f017fb12d90f5cbe763e6", ".tools/engine/external/recomp-net"),
}


def extract(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as file:
        members = file.infolist()
        prefix = members[0].filename.split("/")[0] + "/"
        for member in members:
            relative = member.filename.removeprefix(prefix)
            if not relative or member.is_dir():
                continue
            target = (destination / relative).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise ValueError("Archive path escapes destination")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(file.read(member))


def main():
    downloads = ROOT / ".tools/downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    lock_path = ROOT / "tools/runtime-sources.lock.json"
    recorded = json.loads(lock_path.read_text()) if lock_path.exists() else {}
    result = {}
    for name, (repo, commit, directory) in SOURCES.items():
        url = f"https://codeload.github.com/{repo}/zip/{commit}"
        archive = downloads / f"{name}-{commit}.zip"
        if not archive.exists():
            print("Downloading", name, flush=True)
            urllib.request.urlretrieve(url, archive)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if name in recorded and recorded[name]["sha256"] != digest:
            raise ValueError(f"SHA256 mismatch for {name}")
        extract(archive, ROOT / directory)
        result[name] = {"repository": repo, "commit": commit, "sha256": digest,
                        "url": url, "destination": directory}
        print(name, commit, digest, flush=True)
    if result != recorded:
        lock_path.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
