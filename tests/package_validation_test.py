from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "plugins/mosh.koplugin/_meta.lua",
    "plugins/mosh.koplugin/main.lua",
    "plugins/mosh.koplugin/bin/mosh",
    "plugins/mosh.koplugin/bin/mosh-client",
    "plugins/mosh.koplugin/share/terminfo/v/vt52",
    "plugins/mosh.koplugin/share/terminfo/76/vt52",
    "plugins/mosh.koplugin/README.md",
    "plugins/mosh.koplugin/COMPATIBILITY.md",
    "plugins/mosh.koplugin/OWNER",
    "plugins/mosh.koplugin/docs/security.md",
    "plugins/mosh.koplugin/target-info.json",
]


class PackageValidationTest(unittest.TestCase):
    def test_validate_package_accepts_required_layout_and_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            archive = tmp_path / "pkg.zip"
            manifest = tmp_path / "manifest.json"
            files = []

            with zipfile.ZipFile(archive, "w") as zf:
                for name in REQUIRED:
                    data = f"fixture for {name}\n".encode()
                    zf.writestr(name, data)
                    files.append({
                        "path": name,
                        "size": len(data),
                        "sha256": hashlib.sha256(data).hexdigest(),
                    })

            manifest.write_text(json.dumps({
                "name": "koreader-mosh",
                "version": "0.1.0",
                "target": "kobo",
                "files": files,
            }), encoding="utf-8")

            subprocess.check_call([
                str(ROOT / "scripts" / "validate-package.py"),
                "--archive", str(archive),
                "--manifest", str(manifest),
                "--target", "kobo",
            ])


if __name__ == "__main__":
    unittest.main()
