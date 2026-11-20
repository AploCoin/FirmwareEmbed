#!/usr/bin/env python3
"""Host-only release safety regressions. No serial ports or flash writes."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / f"{name}.py")
    loaded = importlib.util.module_from_spec(spec)
    sys.modules[name] = loaded
    spec.loader.exec_module(loaded)
    return loaded

artifacts, flash = module("artifacts"), module("flash")


class ReleaseSafety(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.project = artifacts.read_project(ROOT / "Firmware/ESP32")
        self.build = self.path / "build"
        self.build.mkdir()
        for name in ("firmware.bin", "bootloader.bin"):
            (self.build / name).write_bytes(b"image" * 100)
        (self.build / "identity.json").write_text(json.dumps(dict(
            board=self.project.id, version="2.0", layout=self.project.layout,
            flashBytes=self.project.flash_bytes,
            storage=dict(offset=self.project.config_offset, size=self.project.config_bytes))))
        self.table()
        self.log = self.path / "build.log"
        self.log.write_text(f"RAM: [=] 10% (used 40000 bytes from 327680 bytes)\nFlash: [=] 40% (used 1000000 bytes from {self.project.config_offset - 0x10000} bytes)\n")
        self.staged = self.path / "staged"
        self.staged.mkdir()
        artifacts.stage(self.project, self.build, self.log, self.path, self.staged, "2.0")
        self.profile = json.loads((self.staged / f"{self.project.id}.json").read_text())
        self.manifest = self.staged / "manifest.json"
        self.write_manifest()
        self.info = dict(board=self.project.id, layout=self.project.layout,
                         flash_bytes=self.project.flash_bytes, profile_flash_bytes=self.project.flash_bytes,
                         storage_offset=self.project.config_offset, storage_size=self.project.config_bytes,
                         storage_schema=2, version="1.9")

    def table(self, extra=()):
        entries = [("nvs", 1, 2, 0x9000, 0x5000),
                   ("factory", 0, 0, 0x10000, self.project.config_offset - 0x10000),
                   ("aplo_cfg", 1, 2, self.project.config_offset, self.project.config_bytes), *extra]
        raw = b"".join(struct.pack("<HBBII16sI", 0x50aa, t, s, start, size, name.encode(), 0) for name, t, s, start, size in entries)
        (self.build / "partitions.bin").write_bytes(raw + b"\xff" * 32)

    def write_manifest(self):
        self.manifest.write_text(json.dumps(dict(schemaVersion=2, eraseAll=False, ota=False,
                                                version="2.0", profiles=[self.profile])))

    def plan(self, **kwargs):
        return flash.plan(self.manifest, self.project.id, "update", self.info, **kwargs)

    def test_profiles_and_immutable_lock(self):
        profiles = artifacts.projects(ROOT / "Firmware")
        self.assertEqual(len(profiles), 8)
        self.assertEqual(len(artifacts.projects(ROOT / "Firmware", True)), 10)
        self.assertNotIn("esp01-512k", {p.id for p in profiles})
        lock = artifacts.read_lock(ROOT / "dependencies.lock.json")
        self.assertEqual(set(lock), artifacts.LIBRARIES)
        malformed = self.path / "lock.json"
        malformed.write_text(json.dumps(dict(AploEmbed="main", MiningEmbed="a" * 40)))
        with self.assertRaises(ValueError):
            artifacts.read_lock(malformed)

    def test_factory_install_omits_ota_selector(self):
        self.assertEqual([p["offset"] for p in self.profile["install"]], [0x1000, 0x8000, 0x10000])
        self.assertEqual(len(self.profile["update"]), 1)
        self.assertFalse(self.profile["ota"])

    def test_only_application_update(self):
        _, parts = self.plan()
        self.assertEqual([offset for offset, _ in parts], [0x10000])
        self.profile["update"] = self.profile["install"]
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.plan()

    def test_chip_family_cannot_select_board_or_layout(self):
        for field, value in (("board", "esp32-wroom-32u"), ("layout", "old-dual-app"),
                             ("storage_offset", 0x190000), ("storage_size", 8192),
                             ("profile_flash_bytes", 2 * 1024 * 1024), ("flash_bytes", 1024)):
            with self.subTest(field=field):
                info = {**self.info, field: value}
                with self.assertRaises(ValueError):
                    flash.plan(self.manifest, self.project.id, "update", info)

    def test_unknown_schema_and_absent_info_rejected(self):
        with self.assertRaises(ValueError):
            flash.plan(self.manifest, self.project.id, "update")
        self.info["storage_schema"] = 99
        with self.assertRaisesRegex(ValueError, "rollback"):
            self.plan()

    def test_corrupt_asset_and_traversal_rejected(self):
        asset = self.staged / self.profile["update"][0]["path"]
        asset.write_bytes(b"x" * asset.stat().st_size)
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            self.plan()
        self.profile["update"][0]["path"] = "../firmware.bin"
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, "regular file"):
            self.plan()

    def test_sector_rounding_protects_configuration(self):
        oversized = self.project.config_offset - 0x10000 + 1
        (self.build / "firmware.bin").write_bytes(b"x" * oversized)
        with self.assertRaisesRegex(ValueError, "sector-bounded"):
            artifacts.stage(self.project, self.build, self.log, self.path, self.path / "bad", "2.0")
        # A non-aligned byte limit cannot borrow the adjacent erase sector.
        project = artifacts.read_project(ROOT / "Firmware/ESP8266", "esp01-1m")
        log = self.path / "8266.log"
        log.write_text("RAM: [=] 20% (used 36000 bytes from 81920 bytes)\nFlash: [=] 50% (used 400000 bytes from 892912 bytes)\n")
        (self.build / "firmware.bin").write_bytes(b"x" * 892912)
        (self.build / "identity.json").write_text(json.dumps(dict(
            board=project.id, version="dev", layout=project.layout, flashBytes=project.flash_bytes,
            storage=dict(offset=project.config_offset, size=project.config_bytes))))
        with self.assertRaisesRegex(ValueError, "sector-bounded"):
            artifacts.stage(project, self.build, log, self.path, self.path / "bad2")

    def test_ota_partition_and_ram_budget_rejected(self):
        self.table([("otadata", 1, 0, 0xe000, 0x2000)])
        with self.assertRaisesRegex(ValueError, "no OTA"):
            artifacts.stage(self.project, self.build, self.log, self.path, self.path / "bad", "2.0")
        project = artifacts.read_project(ROOT / "Firmware/ESP8266")
        self.log.write_text("RAM: [=] 60% (used 44000 bytes from 81920 bytes)\nFlash: [=] 50% (used 400000 bytes from 1044464 bytes)\n")
        with self.assertRaisesRegex(ValueError, "budget"):
            artifacts.measure(self.log, project)

    def test_migration_only_preserves_known_legacy_offset(self):
        _, parts = flash.plan(self.manifest, self.project.id, "migrate", from_layout="esp32-4m-dual-app-v1")
        self.assertEqual(len(parts), 3)
        with self.assertRaises(ValueError):
            flash.plan(self.manifest, self.project.id, "migrate", from_layout="unrelated")

    def test_info_payload_and_json(self):
        self.assertEqual(flash.device_info("APLO1 OK info board=esp32dev storage_schema=2")["board"], "esp32dev")
        self.assertEqual(flash.device_info(json.dumps(self.info)), self.info)

    def test_full_release_assembly_and_checksums(self):
        staged = self.path / "matrix"
        capacities = {"esp12e": 1044464, "esp8266-2m": 1044464, "esp01-1m": 892912}
        for project in artifacts.projects(ROOT / "Firmware"):
            build = self.path / ("build-" + project.id)
            build.mkdir()
            (build / "firmware.bin").write_bytes(b"firmware" * 100)
            (build / "bootloader.bin").write_bytes(b"boot" * 100)
            (build / "identity.json").write_text(json.dumps(dict(
                board=project.id, version="2.0", layout=project.layout, flashBytes=project.flash_bytes,
                storage=dict(offset=project.config_offset, size=project.config_bytes))))
            if project.chip != "ESP8266":
                entries = [("nvs", 1, 2, 0x9000, 0x5000),
                           ("factory", 0, 0, 0x10000, project.config_offset - 0x10000),
                           ("aplo_cfg", 1, 2, project.config_offset, project.config_bytes)]
                raw = b"".join(struct.pack("<HBBII16sI", 0x50aa, t, sub, start, size, name.encode(), 0)
                               for name, t, sub, start, size in entries)
                (build / "partitions.bin").write_bytes(raw + b"\xff" * 32)
            capacity = capacities.get(project.id, project.config_offset - 0x10000)
            log = build / "build.log"
            log.write_text(f"RAM: [=] (used 36000 bytes from 81920 bytes)\nFlash: [=] (used 400000 bytes from {capacity} bytes)\n")
            artifacts.stage(project, build, log, self.path, staged / project.id, "2.0")
        output = self.path / "release"
        artifacts.assemble(ROOT / "Firmware", staged, output, "2.0", artifacts.read_lock(ROOT / "dependencies.lock.json"))
        manifest = json.loads((output / "manifest.json").read_text())
        self.assertEqual(len(manifest["profiles"]), 8)
        self.assertFalse(manifest["eraseAll"])
        for line in (output / "SHA256SUMS").read_text().splitlines():
            digest, filename = line.split("  ")
            self.assertEqual(artifacts.digest(output / filename), digest)
        profile = manifest["profiles"][0]
        info = dict(board=profile["id"], layout=profile["layout"], flash_bytes=profile["flashBytes"],
                    profile_flash_bytes=profile["flashBytes"], storage_offset=profile["storage"]["offset"],
                    storage_size=profile["storage"]["size"], storage_schema=2)
        flash.plan(output / "manifest.json", profile["id"], "update", info)
        # A staged image changed after hashing must never be published.
        corrupt = staged / profile["id"] / profile["install"][0]["path"]
        corrupt.write_bytes(b"bad")
        with self.assertRaisesRegex(ValueError, "corrupt"):
            artifacts.assemble(ROOT / "Firmware", staged, self.path / "corrupt-release", "2.0", manifest["libraries"])

    def test_pin_project_requires_exact_urls(self):
        target = self.path / "project"
        target.mkdir()
        ini = target / "platformio.ini"
        ini.write_text((ROOT / "Firmware/ESP32/platformio.ini").read_text())
        lock = artifacts.read_lock(ROOT / "dependencies.lock.json")
        artifacts.pin_project(target, lock)
        artifacts.pin_project(target, lock)
        self.assertIn(".git#" + lock["AploEmbed"], ini.read_text())

if __name__ == "__main__":
    unittest.main()
