"""Synthetic fixtures only; no commercial ROM data is used by these tests."""
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import tempfile
import tomllib
import unittest
import zipfile
from unittest.mock import patch
from types import SimpleNamespace

PROJECT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("rr", PROJECT / "tools" / "rr.py")
rr = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rr)


def synthetic_rom(title="ROAD RASH"):
    data = bytearray(4096)
    struct.pack_into(">II", data, 0, 0xFFFE00, 0x200)
    for vector in range(2, 64):
        struct.pack_into(">I", data, vector * 4, 0x210)
    data[0x100:0x110] = b"SEGA GENESIS    "
    for start in (0x120, 0x150):
        data[start:start + 48] = title.encode("ascii").ljust(48, b" ")
    data[0x180:0x18E] = b"GM TEST0000-00"
    struct.pack_into(">II", data, 0x1A0, 0, len(data) - 1)
    data[0x1F0:0x200] = b"JUE             "
    # moveq #1,d0; addq.w #1,d0; bra.s back to addq; exception handler: rte.
    data[0x200:0x206] = bytes.fromhex("7001 5240 60fc")
    data[0x210:0x212] = bytes.fromhex("4e73")
    checksum = sum(x[0] for x in struct.iter_unpack(">H", data[0x200:])) & 0xFFFF
    struct.pack_into(">H", data, 0x18E, checksum)
    return bytes(data)


class AnalysisTests(unittest.TestCase):
    def test_diagnostics_keep_discovery_gaps_visible(self):
        log = "Final function count after boundary splitting: 563\nWrote 9 unresolved sites\n  TOTAL 0\n"
        self.assertEqual(rr.codegen_diagnostics(log), {
            "emitted_functions": 563, "unresolved_dispatch_sites": 9, "unsupported_events": 0})

    def test_header_checksum_and_vectors(self):
        report = rr.analyze(synthetic_rom())
        self.assertEqual(report["title_hint"], "rr1")
        self.assertTrue(report["checksum_matches"])
        self.assertEqual(report["reset_pc"], 0x200)
        self.assertEqual(report["initial_ssp"], 0xFFFE00)
        self.assertEqual({v["address"] for v in report["vectors"] if v["rom_candidate"]}, {0x200, 0x210})
        self.assertEqual(report["warnings"], [])

    def test_all_title_variants(self):
        for title, game in (("ROAD RASH", "rr1"), ("ROAD RASH II", "rr2"),
                            ("ROAD RASH 2", "rr2"), ("ROAD RASH 3", "rr3"),
                            ("ROAD RASH III", "rr3")):
            with self.subTest(title=title):
                self.assertEqual(rr.analyze(synthetic_rom(title))["title_hint"], game)
        self.assertIsNone(rr.title_hint("SOME OTHER GAME"))

    def test_truncated_odd_and_non_genesis_rejected(self):
        for data in (b"", synthetic_rom()[:-1], bytes(4096)):
            with self.subTest(length=len(data)), self.assertRaises(rr.ProjectError):
                rr.analyze(data)

    def test_copier_header_and_swapped_bytes_rejected(self):
        original = synthetic_rom()
        swapped = bytearray(original)
        swapped[0::2], swapped[1::2] = original[1::2], original[0::2]
        for data in (bytes(512) + original, bytes(swapped)):
            with self.assertRaises(rr.ProjectError):
                rr.analyze(data)

    def test_checksum_mismatch_is_reported(self):
        data = bytearray(synthetic_rom())
        data[-1] ^= 1
        report = rr.analyze(data)
        self.assertFalse(report["checksum_matches"])
        self.assertTrue(report["warnings"])

    def test_invalid_reset_rejected(self):
        for address in (0, 0x100, 0x201, 0x100000):
            data = bytearray(synthetic_rom())
            struct.pack_into(">I", data, 4, address)
            with self.subTest(address=address), self.assertRaises(rr.ProjectError):
                rr.analyze(data)

    def test_invalid_exception_vectors_are_not_seeded(self):
        data = bytearray(synthetic_rom())
        for i, value in enumerate((0, 0xFFFFFFFF, 0x211, 0xFF0000), 2):
            struct.pack_into(">I", data, 4 * i, value)
        report = rr.analyze(data)
        self.assertFalse(any(v["rom_candidate"] for v in report["vectors"][1:5]))


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="rr-pipeline-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(PROJECT / "games", self.root / "games")
        (self.root / "tools").mkdir()
        shutil.copyfile(PROJECT / "tools" / "tools.lock.json", self.root / "tools" / "tools.lock.json")
        self.source = self.root / "Original ROM with spaces.bin"
        self.source.write_bytes(synthetic_rom())

    def test_import_preserves_source_and_binds_profile(self):
        original = self.source.read_bytes()
        state = rr.import_rom(self.root, "rr1", self.source)
        self.assertEqual(self.source.read_bytes(), original)
        self.assertEqual((self.root / state["rom"]).read_bytes(), original)
        self.assertFalse(state["qualified"])
        config_path = self.root / state["config"]
        config = tomllib.loads(config_path.read_text())
        self.assertEqual(config["ram_layout"], {"initial_ssp": 0xFFFE00})
        self.assertEqual(config["functions"]["extra"], [0x200, 0x210])
        evidence = config_path.parent / config["game"]["discovery_files"][0]
        self.assertTrue(evidence.is_file())
        config_path.write_text(config_path.read_text() + "# Manual evidence\n")
        rr.import_rom(self.root, "rr1", self.source)
        self.assertIn("# Manual evidence", config_path.read_text())

    def test_wrong_game_rejected_before_writing(self):
        with self.assertRaises(rr.ProjectError):
            rr.import_rom(self.root, "rr2", self.source)
        self.assertFalse((self.root / ".local").exists())

    def test_zip_import_preserves_archive_and_hash(self):
        archive = self.root / "Road Rash.zip"
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as file:
            file.writestr("nested/Road Rash.md", self.source.read_bytes())
        original = archive.read_bytes()
        state = rr.import_rom(self.root, "rr1", archive)
        self.assertEqual(archive.read_bytes(), original)
        self.assertEqual(state["report"]["sha256"], rr.inspect_rom(self.source)["sha256"])
        self.assertEqual(state["report"]["source"]["archive_member"], "nested/Road Rash.md")

    def test_ambiguous_and_invalid_zip_rejected(self):
        archive = self.root / "ambiguous.zip"
        with zipfile.ZipFile(archive, "w") as file:
            file.writestr("a.md", self.source.read_bytes())
            file.writestr("b.md", self.source.read_bytes())
        with self.assertRaisesRegex(rr.ProjectError, "exatamente uma"):
            rr.inspect_rom(archive)
        archive.write_bytes(b"invalid zip")
        with self.assertRaises(rr.ProjectError):
            rr.inspect_rom(archive)

    def test_discovery_addresses_need_qualified_hash(self):
        discovery = self.root / "games" / "rr1" / "discovery.toml"
        discovery.write_text("[functions]\nextra = [0x220]\n")
        with self.assertRaises(rr.ProjectError):
            rr.import_rom(self.root, "rr1", self.source)

    def test_modified_copy_rejected_before_codegen(self):
        state = rr.import_rom(self.root, "rr1", self.source)
        copied = self.root / state["rom"]
        data = bytearray(copied.read_bytes())
        data[-1] ^= 1
        copied.write_bytes(data)
        with self.assertRaisesRegex(rr.ProjectError, "copia local"):
            rr.generate(self.root, "rr1")

    def test_evidence_checks_hash_and_bytes(self):
        state = rr.import_rom(self.root, "rr1", self.source)
        evidence = {"rom_sha256": state["report"]["sha256"], "ram_candidates": {},
                    "byte_checks": [{"name": "reset", "offset": "0x4", "expected_hex": "00000200"}]}
        path = self.root / "games" / "rr1" / "evidence.json"
        rr.write_json(path, evidence)
        self.assertTrue(rr.verify_evidence(self.root, "rr1")["checks"][0]["matches"])
        evidence["byte_checks"][0]["expected_hex"] = "00000300"
        rr.write_json(path, evidence)
        with self.assertRaises(rr.ProjectError):
            rr.verify_evidence(self.root, "rr1")
        evidence["rom_sha256"] = "0" * 64
        rr.write_json(path, evidence)
        with self.assertRaisesRegex(rr.ProjectError, "Hash"):
            rr.verify_evidence(self.root, "rr1")

    def install_test_tool(self):
        real = rr.recompiler_path(PROJECT)
        if not real.is_file():
            self.skipTest("Pinned GenesisRecomp is not prepared in this source checkout")
        shutil.copytree(real.parent, rr.recompiler_path(self.root).parent)

    def test_real_static_codegen_on_synthetic_rom(self):
        self.install_test_tool()
        rr.import_rom(self.root, "rr1", self.source)
        report = rr.generate(self.root, "rr1")
        self.assertEqual(report["status"], "experimental_codegen_passed")
        self.assertFalse(report["playable"])
        output = Path(report["output"])
        self.assertTrue((output / "generation.json").is_file())
        combined = "\n".join(path.read_text() for path in output.glob("rr1_part*.c"))
        self.assertIn("func_000200", combined)
        self.assertIn("--fail-on-unsupported", report["command"])

    def test_failed_codegen_records_failure_and_uses_fresh_directory(self):
        self.install_test_tool()
        rr.import_rom(self.root, "rr1", self.source)
        old = self.root / "build" / "generated" / "rr1" / "old"
        old.mkdir(parents=True)
        (old / "rr1_part00.c").write_text("/* stale output */")
        with patch.object(rr.subprocess, "run", return_value=SimpleNamespace(returncode=2)):
            with self.assertRaises(rr.ProjectError):
                rr.generate(self.root, "rr1")
        reports = list(old.parent.glob("*/generation.json"))
        self.assertEqual(len(reports), 1)
        self.assertEqual(json.loads(reports[0].read_text())["status"], "codegen_failed")


if __name__ == "__main__":
    unittest.main()
