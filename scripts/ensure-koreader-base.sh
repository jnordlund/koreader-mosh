#!/bin/sh
set -eu

root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cache_dir="${root}/${CACHE_DIR:-.cache}"
revision="${KOREADER_BASE_REVISION:-e26612880d468c45e6bea6805ae422840032dd66}"

if [ -n "${KOREADER_BASE:-}" ] && [ -f "${KOREADER_BASE}/Makefile.defs" ]; then
    printf '%s\n' "${KOREADER_BASE}"
    exit 0
fi

if [ -f "${root}/../base/Makefile.defs" ]; then
    printf '%s\n' "${root}/../base"
    exit 0
fi

base_dir="${cache_dir}/koreader-base-${revision}"
if [ ! -f "${base_dir}/Makefile.defs" ]; then
    mkdir -p "${base_dir}"
    url="https://raw.githubusercontent.com/koreader/koreader-base/${revision}/Makefile.defs"
    curl -L -sS -o "${base_dir}/Makefile.defs.tmp" "${url}"
    mv "${base_dir}/Makefile.defs.tmp" "${base_dir}/Makefile.defs"
fi

printf '%s\n' "${base_dir}"
