#!/usr/bin/env python3
"""Extract Kobo target metadata from koreader-base Makefile.defs.

The real build still uses the target compiler. This parser exists so the
metadata, docs, and tests do not silently drift from koreader-base when the
compiler is not installed on the host running the fast test suite.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path


SUPPORTED_TARGETS = {"kobo", "kobov4", "kobov5"}
TARGET_ARCH_VARS = {
    "kobo": "ARMV7_A8_ARCH",
    "kobov4": "ARMV7_A9_ARCH",
    "kobov5": "ARMV7_A53_ARCH",
}
DEFAULT_CHOST = {
    "kobo": "arm-kobo-linux-gnueabihf",
    "kobov4": "arm-kobov4-linux-gnueabihf",
    "kobov5": "arm-kobov5-linux-gnueabihf",
}
FALLBACK_CHOST = "arm-linux-gnueabihf"


def read_defs(path: Path) -> str:
    defs = path / "Makefile.defs"
    if not defs.is_file():
        raise SystemExit(f"missing koreader-base Makefile.defs at {defs}")
    return defs.read_text(encoding="utf-8")


def extract_assignment(text: str, name: str) -> str:
    match = re.search(rf"^{re.escape(name)}:?=(.+)$", text, re.MULTILINE)
    if not match:
        raise SystemExit(f"could not find {name} in Makefile.defs")
    return match.group(1).strip()


def extract_target_block(text: str, target: str) -> str:
    start_re = re.compile(rf"^(?:else )?ifeq \(\$\(TARGET\), {re.escape(target)}\)$", re.MULTILINE)
    start = start_re.search(text)
    if not start:
        raise SystemExit(f"could not find target block for {target}")
    end = re.search(r"^else ifeq \(\$\(TARGET\), |^else ifneq |^else ifndef |^endif", text[start.end():], re.MULTILINE)
    return text[start.end(): start.end() + end.start()] if end else text[start.end():]


def extract_glibc(block: str) -> str:
    match = re.search(r"GLIBC_VERSION_MAX\s*=\s*([0-9.]+)", block)
    if not match:
        raise SystemExit("could not find GLIBC_VERSION_MAX in target block")
    return match.group(1)


def compiler_machine(chost: str) -> str | None:
    compiler = shutil.which(f"{chost}-gcc")
    if not compiler:
        return None
    try:
        return subprocess.check_output([compiler, "-dumpmachine"], text=True).strip() or None
    except (OSError, subprocess.CalledProcessError):
        return None


def target_info(target: str, base: Path) -> dict[str, object]:
    if target not in SUPPORTED_TARGETS:
        raise SystemExit(f"unsupported target {target}; expected one of {', '.join(sorted(SUPPORTED_TARGETS))}")

    text = read_defs(base)
    block = extract_target_block(text, target)
    requested_chost = DEFAULT_CHOST[target]
    chost = requested_chost if shutil.which(f"{requested_chost}-gcc") else FALLBACK_CHOST
    arch_var = TARGET_ARCH_VARS[target]
    arch_flags = extract_assignment(text, arch_var)
    cflags = f"{arch_flags} -mfloat-abi=hard"
    compat_flags = extract_assignment(text, "UBUNTU_COMPAT_CFLAGS") if chost == FALLBACK_CHOST else ""
    glibc = extract_glibc(block)
    machine = compiler_machine(chost) or chost

    return {
        "target": target,
        "chost": chost,
        "requested_chost": requested_chost,
        "compiler": f"{chost}-gcc",
        "cxx": f"{chost}-g++",
        "strip": f"{chost}-strip",
        "ar": f"{chost}-ar",
        "ranlib": f"{chost}-ranlib",
        "target_machine": machine,
        "glibc_version_max": glibc,
        "arch_var": arch_var,
        "arch_flags": arch_flags,
        "cflags": " ".join(part for part in [cflags, compat_flags] if part),
        "cxxflags": " ".join(part for part in [cflags, compat_flags, "-std=gnu++17"] if part),
        "ldflags": "-Wl,-O1,--as-needed,--gc-sections -Wl,-z,noexecstack -Wl,--build-id=none",
        "compatible_with": [target],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True)
    parser.add_argument("--koreader-base", default="../base")
    parser.add_argument("--shell", action="store_true")
    args = parser.parse_args()

    info = target_info(args.target, Path(args.koreader_base))
    if args.shell:
        for key, value in info.items():
            if isinstance(value, list):
                value = " ".join(value)
            print(f"{key.upper()}={value}")
    else:
        print(json.dumps(info, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
