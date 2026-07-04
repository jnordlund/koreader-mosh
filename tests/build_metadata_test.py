from __future__ import annotations

import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURE_BASE = ROOT / "tests" / "fixtures" / "koreader-base"


class BuildMetadataTest(unittest.TestCase):
    def target_info(self, target: str) -> dict:
        out = subprocess.check_output([
            str(ROOT / "scripts" / "target-info.py"),
            "--target", target,
            "--koreader-base", str(FIXTURE_BASE),
        ], text=True)
        return json.loads(out)

    def test_kobo_target_metadata_comes_from_makefile_defs(self):
        info = self.target_info("kobo")
        self.assertEqual(info["target"], "kobo")
        self.assertEqual(info["arch_var"], "ARMV7_A8_ARCH")
        self.assertIn("-mtune=cortex-a8", info["cflags"])
        self.assertIn("-mfloat-abi=hard", info["cflags"])
        self.assertEqual(info["glibc_version_max"], "2.15")

    def test_kobov4_and_kobov5_are_separate_cpu_tunes(self):
        v4 = self.target_info("kobov4")
        v5 = self.target_info("kobov5")
        self.assertIn("-mtune=cortex-a9", v4["cflags"])
        self.assertIn("-mtune=cortex-a53", v5["cflags"])
        self.assertNotEqual(v4["cflags"], v5["cflags"])


if __name__ == "__main__":
    unittest.main()
