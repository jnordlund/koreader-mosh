#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument(
        "--koreader-base-revision",
        default=os.environ.get("KOREADER_BASE_REVISION", "e26612880d468c45e6bea6805ae422840032dd66"),
    )
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    stage = Path(args.stage)
    files = []
    for path in sorted(p for p in stage.rglob("*") if p.is_file()):
        files.append({
            "path": path.relative_to(stage).as_posix(),
            "size": path.stat().st_size,
            "sha256": sha256(path),
        })

    manifest = {
        "name": "koreader-mosh",
        "version": args.version,
        "target": args.target,
        "files": files,
        "licenses": ["GPL-3.0-or-later"],
        "upstreams": {
            "mosh": "mosh-1.4.0",
            "koreader-base": args.koreader_base_revision,
        },
    }
    output = Path(args.output)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
