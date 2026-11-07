"""Fetch cover references and the OFL display font, retaining exact origins."""
from pathlib import Path
import urllib.request
import urllib.parse
import json
import hashlib

ROOT = Path(__file__).resolve().parents[1]
assets = ROOT / "assets/launcher"
assets.mkdir(parents=True, exist_ok=True)
sources = {
    "rr1.png": ("libretro-thumbnails/Sega_-_Mega_Drive_-_Genesis", "master", "Named_Boxarts/Road Rash (USA, Europe).png"),
    "rr2.png": ("libretro-thumbnails/Sega_-_Mega_Drive_-_Genesis", "master", "Named_Boxarts/Road Rash II (USA, Europe) (RR206).png"),
    "rr3.png": ("libretro-thumbnails/Sega_-_Mega_Drive_-_Genesis", "master", "Named_Boxarts/Road Rash 3 (USA, Europe).png"),
    "BarlowCondensed-Bold.ttf": ("google/fonts", "main", "ofl/barlowcondensed/BarlowCondensed-Bold.ttf"),
    "FONT-LICENSE.txt": ("google/fonts", "main", "ofl/barlowcondensed/OFL.txt"),
    "Barlow-Regular.ttf": ("google/fonts", "main", "ofl/barlow/Barlow-Regular.ttf"),
    "BARLOW-LICENSE.txt": ("google/fonts", "main", "ofl/barlow/OFL.txt"),
}
ledger_path=assets/"sources.json"
ledger = json.loads(ledger_path.read_text()) if ledger_path.exists() else {}
for name, (repo, branch, path) in sources.items():
    url = f"https://raw.githubusercontent.com/{repo}/{branch}/" + urllib.parse.quote(path)
    target = assets / name
    if not target.exists():
        urllib.request.urlretrieve(url, target)
    ledger[name] = dict(ledger.get(name,{}),url=url,sha256=hashlib.sha256(target.read_bytes()).hexdigest())
    print(name, target.stat().st_size)
(assets / "sources.json").write_text(json.dumps(ledger, indent=2) + "\n")
