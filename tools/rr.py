"""Road Rash Trilogy: ROM intake and experimental static code-generation pipeline.

Only Python's standard library is required. This is not a playable game runner.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
import uuid
import zlib
import zipfile

ROOT = Path(__file__).resolve().parents[1]
GAMES = ("rr1", "rr2", "rr3")


class ProjectError(Exception):
    pass


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    # Same-directory replace avoids publishing a partially written manifest.
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent,
                                     delete=False, suffix=".tmp") as file:
        temp = Path(file.name)
        json.dump(data, file, ensure_ascii=False, indent=2)
        file.write("\n")
    try:
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def now() -> str:
    return datetime.now(timezone.utc).isoformat()


def title_hint(*titles: str) -> str | None:
    title = " ".join(titles).upper()
    if not re.search(r"\bROAD\s+RASH\b", title):
        return None
    if re.search(r"\bROAD\s+RASH\s*(?:3|III)\b", title):
        return "rr3"
    if re.search(r"\bROAD\s+RASH\s*(?:2|II)\b", title):
        return "rr2"
    return "rr1"


def analyze(data: bytes) -> dict:
    """Parse an unmodified, linear, big-endian cartridge dump."""
    if len(data) < 0x200:
        raise ProjectError("Arquivo menor que o cabecalho Mega Drive (512 bytes).")
    if len(data) % 2:
        raise ProjectError("Dump com tamanho impar; esperado um dump binario de palavras 68000.")
    if data[0x100:0x104] != b"SEGA":
        if data[0x100:0x104] == b"ESAG":
            raise ProjectError("Dump com bytes trocados. Use um dump big-endian.")
        if data[0x300:0x304] == b"SEGA":
            raise ProjectError("Cabecalho de copiador de 512 bytes detectado; use um dump sem cabecalho.")
        raise ProjectError("Assinatura SEGA ausente em 0x100. SMD/ZIP nao sao aceitos; use BIN/MD/GEN linear.")
    if len(data) < 0x202:
        raise ProjectError("Dump sem instrucoes apos o cabecalho.")

    def text(start: int, length: int) -> str:
        return data[start:start + length].decode("ascii", errors="replace").strip(" \x00")

    def u32(offset: int) -> int:
        return struct.unpack_from(">I", data, offset)[0]

    checksum = sum(word[0] for word in struct.iter_unpack(">H", data[0x200:])) & 0xFFFF
    expected = struct.unpack_from(">H", data, 0x18E)[0]
    reset_raw = u32(4)
    reset = reset_raw & 0xFFFFFF
    if reset % 2 or not 0x200 <= reset <= len(data) - 2:
        raise ProjectError(f"Vetor RESET fora da area de codigo ROM: 0x{reset_raw:08X}.")
    vectors = []
    for index in range(1, 64):
        raw = u32(index * 4)
        address = raw & 0xFFFFFF
        vectors.append({
            "index": index, "offset": index * 4, "raw": raw, "address": address,
            "rom_candidate": raw != 0xFFFFFFFF and address % 2 == 0
                             and 0x200 <= address <= len(data) - 2,
        })
    warnings = []
    if checksum != expected:
        warnings.append("Checksum interno divergente: dump modificado ou checksum original incorreto; investigar.")
    if u32(0x1A0) != 0 or u32(0x1A4) != len(data) - 1:
        warnings.append("Faixa ROM declarada no cabecalho difere do tamanho do arquivo.")
    sp = u32(0)
    if not 0xFF0000 <= (sp & 0xFFFFFF) <= 0xFFFFFF and sp != 0x1000000:
        warnings.append("SSP inicial fora da RAM alta esperada; verificar espelhamento.")
    domestic, overseas = text(0x120, 48), text(0x150, 48)
    return {
        "schema": 1, "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "sha1": hashlib.sha1(data).hexdigest(),
        "crc32": f"{zlib.crc32(data):08X}",
        "console": text(0x100, 16), "domestic_title": domestic,
        "overseas_title": overseas, "serial": text(0x180, 14),
        "region": text(0x1F0, 16), "initial_ssp": sp,
        "reset_pc": reset, "checksum_header": expected,
        "checksum_computed": checksum, "checksum_matches": checksum == expected,
        "title_hint": title_hint(domestic, overseas),
        "identity_confidence": "header_hint_only",
        "vectors": vectors, "warnings": warnings,
    }


def read_rom(path: Path) -> tuple[bytes, dict]:
    if not path.is_file():
        raise ProjectError(f"ROM nao encontrada: {path}")
    limit = 32 * 1024 * 1024
    if path.suffix.lower() == ".zip":
        try:
            with zipfile.ZipFile(path) as archive:
                candidates = [i for i in archive.infolist() if not i.is_dir()
                              and Path(i.filename).suffix.lower() in (".bin", ".md", ".gen")]
                if len(candidates) != 1:
                    raise ProjectError("ZIP deve conter exatamente uma ROM BIN/MD/GEN; selecao ambigua ou ausente.")
                entry = candidates[0]
                if entry.file_size > limit:
                    raise ProjectError("ROM no ZIP excede 32 MiB.")
                # Stream into memory with a hard cap. Archive paths are never extracted.
                with archive.open(entry) as stream:
                    data = stream.read(limit + 1)
                if len(data) > limit:
                    raise ProjectError("ROM descompactada excede 32 MiB.")
                return data, {"path": str(path.resolve()), "archive_member": entry.filename}
        except (zipfile.BadZipFile, RuntimeError, NotImplementedError) as error:
            raise ProjectError(f"Nao foi possivel ler o ZIP: {error}") from error
    if path.stat().st_size > limit:
        raise ProjectError("Arquivo maior que 32 MiB; confirme que e um dump de cartucho.")
    return path.read_bytes(), {"path": str(path.resolve()), "archive_member": None}


def inspect_rom(path: Path) -> dict:
    data, source = read_rom(path)
    return {**analyze(data), "source": source}


def import_rom(root: Path, game: str, path: Path) -> dict:
    report = inspect_rom(path)
    if report["title_hint"] != game:
        raise ProjectError(f"Titulo do cabecalho sugere {report['title_hint']!r}, nao {game}. Nenhum arquivo importado.")
    profile = read_json(root / "games" / game / "profile.json")
    qualified = report["sha256"] in profile["qualified_rom_sha256"]
    # Address evidence is revision specific. Shared nonempty discovery is allowed
    # only after this exact input hash has been qualified deliberately.
    discovery = root / "games" / game / "discovery.toml"
    import tomllib
    evidence = tomllib.loads(discovery.read_text(encoding="utf-8"))
    has_evidence = any(evidence.get("functions", {}).values()) or any(
        evidence.get(key) for key in ("jump_table", "protected_range"))
    if has_evidence and not qualified:
        raise ProjectError("Ha enderecos mapeados para outra revisao. Qualifique o SHA256 antes de usa-los.")
    digest = report["sha256"]
    destination = root / ".local" / "roms" / game / f"{digest}.bin"
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Re-read and rehash the exact bytes copied, guarding a changed source.
    data, _ = read_rom(path)
    if hashlib.sha256(data).hexdigest() != digest:
        raise ProjectError("A ROM mudou durante a importacao. Tente novamente.")
    destination.write_bytes(data)
    config = root / ".local" / "profiles" / game / digest / "game.toml"
    config.parent.mkdir(parents=True, exist_ok=True)
    relative_discovery = Path(os.path.relpath(discovery, config.parent)).as_posix()
    seeds = sorted({v["address"] for v in report["vectors"] if v["rom_candidate"]})
    contents = (
        "# EXPERIMENTAL codegen-only profile. Not a qualified game runner.\n"
        f"# Input SHA256: {digest}\n"
        "# Only vector-table candidates are seeded; code coverage is unknown.\n"
        "[game]\n"
        f'output_prefix = "{game}"\n'
        f"discovery_files = [{json.dumps(relative_discovery)}]\n\n"
        "[ram_layout]\n"
        "# Only the initial stack value is known. Other RAM fields remain unmapped.\n"
        f"initial_ssp = 0x{report['initial_ssp']:08X}\n\n"
        "[functions]\n"
        f"extra = [{', '.join(f'0x{s:06X}' for s in seeds)}]\n"
        "blacklist = []\n"
    )
    # Re-import must preserve manual discoveries made for this same revision.
    if not config.exists():
        config.write_text(contents, encoding="utf-8")
    state = {
        "schema": 1, "game": game, "imported_at": now(),
        "rom": destination.relative_to(root).as_posix(),
        "config": config.relative_to(root).as_posix(),
        "qualified": qualified, "report": report,
    }
    write_json(root / ".local" / "games" / f"{game}.json", state)
    return state


def recompiler_path(root: Path) -> Path:
    return root / ".tools" / "genesisrecomp" / "GenesisRecomp.exe"


def codegen_diagnostics(log: str) -> dict:
    functions = re.findall(r"Final function count after boundary splitting:\s*(\d+)", log)
    if not functions:
        functions = re.findall(r"\[GenesisRecomp\] Found (\d+) functions", log)
    unresolved = re.findall(r"Wrote (\d+) unresolved sites", log)
    unsupported = re.findall(r"^\s*TOTAL\s+(\d+)\s*$", log, re.MULTILINE)
    return {
        "emitted_functions": int(functions[-1]) if functions else None,
        "unresolved_dispatch_sites": int(unresolved[-1]) if unresolved else 0,
        "unsupported_events": int(unsupported[-1]) if unsupported else None,
    }


def verify_evidence(root: Path, game: str) -> dict:
    state = read_json(root / ".local" / "games" / f"{game}.json")
    evidence = read_json(root / "games" / game / "evidence.json")
    data = (root / state["rom"]).read_bytes()
    if hashlib.sha256(data).hexdigest() != evidence["rom_sha256"]:
        raise ProjectError("Hash da ROM difere da revisao descrita nas evidencias.")
    checks = []
    for entry in evidence["byte_checks"]:
        offset = int(entry["offset"], 16)
        expected = bytes.fromhex(entry["expected_hex"])
        checks.append({"name": entry["name"], "offset": entry["offset"],
                       "matches": data[offset:offset + len(expected)] == expected})
    if not all(entry["matches"] for entry in checks):
        raise ProjectError("Uma ou mais evidencias diferem da ROM importada.")
    return {"game": game, "rom_sha256": evidence["rom_sha256"], "checks": checks,
            "ram_candidates": evidence["ram_candidates"],
            "ram_validation": "reference_only_not_measured_in_runtime"}


def generate(root: Path, game: str) -> dict:
    state_path = root / ".local" / "games" / f"{game}.json"
    if not state_path.exists():
        raise ProjectError(f"Importe a ROM antes: .\\rr.ps1 import-rom {game} <caminho>")
    state = read_json(state_path)
    rom = root / state["rom"]
    if inspect_rom(rom)["sha256"] != state["report"]["sha256"]:
        raise ProjectError("A copia local da ROM mudou; importe e analise novamente.")
    config = root / state["config"]
    if not config.is_file():
        raise ProjectError(f"Perfil ausente: {config}")
    tool = recompiler_path(root)
    if not tool.is_file():
        raise ProjectError("GenesisRecomp ausente. Execute scripts/setup.ps1.")
    lock = read_json(root / "tools" / "tools.lock.json")["genesisrecomp"]
    build_info = read_json(tool.parent / "build-info.json")
    if build_info.get("source_commit") != lock["source_commit"]:
        raise ProjectError("GenesisRecomp nao corresponde ao commit fixado em tools.lock.json.")
    # Every attempt has its own directory; stale C can never mark a failed run OK.
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    output = root / "build" / "generated" / game / run_id
    output.mkdir(parents=True)
    # Save the exact config and evidence used, alongside the log for reproduction.
    shutil.copyfile(config, output / "input-game.toml")
    shutil.copyfile(root / "games" / game / "discovery.toml", output / "input-discovery.toml")
    command = [str(tool), str(rom), "--game", str(config),
               "--output-dir", str(output), "--fail-on-unsupported"]
    print("Gerando C experimental; cobertura desconhecida. Log: " + str(output / "codegen.log"), flush=True)
    with (output / "codegen.log").open("w", encoding="utf-8") as log:
        result = subprocess.run(command, cwd=config.parent, stdout=log, stderr=subprocess.STDOUT,
                                timeout=300, check=False)
    required = [output / f"{game}_{suffix}" for suffix in ("decls.h", "dispatch.c", "layout.c")]
    emitted = all(p.is_file() for p in required) and any(output.glob(f"{game}_part*.c"))
    success = result.returncode == 0 and emitted
    report = {
        "schema": 1, "game": game, "time": now(), "command": command,
        "rom_sha256": state["report"]["sha256"], "tool": build_info,
        "config_sha256": hashlib.sha256(config.read_bytes()).hexdigest(),
        "returncode": result.returncode, "expected_files_present": emitted,
        "status": "experimental_codegen_passed" if success else "codegen_failed",
        "playable": False, "static_coverage": "unknown", "output": str(output),
        "diagnostics": codegen_diagnostics((output / "codegen.log").read_text(encoding="utf-8", errors="replace")),
    }
    write_json(output / "generation.json", report)
    if not success:
        raise ProjectError(f"Geracao falhou (exit={result.returncode}). Consulte {output / 'codegen.log'}")
    return report


def doctor(root: Path) -> dict:
    tool = recompiler_path(root)
    vs_root = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio"
    compilers = list(vs_root.glob("2022/*/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe"))
    cmakes = list(vs_root.glob("2022/*/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"))
    return {
        "python": sys.version.split()[0], "python_executable": sys.executable,
        "genesisrecomp": str(tool) if tool.is_file() else None,
        "git": shutil.which("git"), "cmake": shutil.which("cmake") or next(map(str, cmakes), None),
        "c_compiler": shutil.which("cl") or next(map(str, compilers), None),
        "generation_ready": tool.is_file(), "playable_runner": False,
        "next": "Importar ROM e gerar codigo experimental; integrar runtime depois.",
    }


def status(root: Path) -> dict:
    games = []
    for game in GAMES:
        profile = read_json(root / "games" / game / "profile.json")
        state_path = root / ".local" / "games" / f"{game}.json"
        entry = {"game": game, "title": profile["title"], "state": "awaiting_rom",
                 "playable": False, "enhancements": "not_implemented"}
        if state_path.is_file():
            state = read_json(state_path)
            entry.update(state="rom_imported", sha256=state["report"]["sha256"],
                         qualified=state["qualified"])
            generations = sorted((root / "build" / "generated" / game).glob("*/generation.json"))
            matching = [read_json(p) for p in generations
                        if read_json(p).get("rom_sha256") == state["report"]["sha256"]]
            if matching:
                last = matching[-1]
                log = Path(last["output"]) / "codegen.log"
                entry["last_generation"] = {
                    "status": last["status"], "time": last["time"],
                    "diagnostics": last.get("diagnostics") or codegen_diagnostics(log.read_text(encoding="utf-8", errors="replace")),
                    "log": str(log),
                }
        games.append(entry)
    return {"phase": "M0 - ROM intake / experimental codegen", "games": games}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("doctor", help="Diagnostico das ferramentas locais")
    sub.add_parser("status", help="Estado real dos tres jogos")
    inspect = sub.add_parser("inspect", help="Inspecionar ROM sem modificar o original")
    inspect.add_argument("rom", type=Path)
    inspect.add_argument("--output", type=Path, help="Salvar relatorio JSON local")
    importer = sub.add_parser("import-rom", help="Copiar ROM para armazenamento privado por SHA256")
    importer.add_argument("game", choices=GAMES)
    importer.add_argument("rom", type=Path)
    gen = sub.add_parser("generate", help="Gerar C experimental; nao produz executavel jogavel")
    gen.add_argument("game", choices=GAMES)
    evidence = sub.add_parser("verify-evidence", help="Conferir bytes de referencias publicas contra a ROM local")
    evidence.add_argument("game", choices=GAMES)
    sub.add_parser("test", help="Testes de integridade e traducao com ROM sintetica")
    args = parser.parse_args(argv)
    try:
        if args.command == "doctor":
            result = doctor(ROOT)
        elif args.command == "status":
            result = status(ROOT)
        elif args.command == "inspect":
            result = inspect_rom(args.rom)
            if args.output:
                if args.output.resolve() == args.rom.resolve():
                    raise ProjectError("O relatorio nao pode sobrescrever a ROM/ZIP original.")
                write_json(args.output, result)
        elif args.command == "import-rom":
            result = import_rom(ROOT, args.game, args.rom)
        elif args.command == "generate":
            result = generate(ROOT, args.game)
        elif args.command == "verify-evidence":
            result = verify_evidence(ROOT, args.game)
        else:
            # Embeddable Python omits the script directory from sys.path.
            sys.path.insert(0, str(ROOT / "tests"))
            import unittest
            suite = unittest.defaultTestLoader.discover(str(ROOT / "tests"))
            return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (ProjectError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"Erro: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
