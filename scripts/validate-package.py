#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path


REQUIRED = {
    "plugins/mosh.koplugin/_meta.lua",
    "plugins/mosh.koplugin/main.lua",
    "plugins/mosh.koplugin/bin/mosh",
    "plugins/mosh.koplugin/bin/mosh-client",
    "plugins/mosh.koplugin/README.md",
    "plugins/mosh.koplugin/COMPATIBILITY.md",
    "plugins/mosh.koplugin/OWNER",
    "plugins/mosh.koplugin/docs/security.md",
    "plugins/mosh.koplugin/target-info.json",
}
TERMINFO_REQUIRED = {
    "plugins/mosh.koplugin/share/terminfo/v/vt52",
    "plugins/mosh.koplugin/share/terminfo/76/vt52",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--target", required=True)
    args = parser.parse_args()

    with zipfile.ZipFile(args.archive) as zf:
        names = set(zf.namelist())
    missing = sorted(REQUIRED - names)
    missing.extend(sorted(TERMINFO_REQUIRED - names))
    if missing:
        raise SystemExit("package missing required files: " + ", ".join(missing))

    manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
    if manifest["target"] != args.target:
        raise SystemExit(f"manifest target {manifest['target']} does not match {args.target}")
    manifest_names = {entry["path"] for entry in manifest["files"]}
    missing_manifest = sorted(REQUIRED - manifest_names)
    missing_manifest.extend(sorted(TERMINFO_REQUIRED - manifest_names))
    if missing_manifest:
        raise SystemExit("manifest missing required files: " + ", ".join(missing_manifest))
    print(f"validated {args.archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
