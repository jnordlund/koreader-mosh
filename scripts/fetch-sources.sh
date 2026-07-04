#!/bin/sh
set -eu

MOSH_VERSION="1.4.0"
MOSH_TAG="mosh-${MOSH_VERSION}"
MOSH_SHA256="ae581fbddf038730af9eee4d319a483288395a0722d0c94c7efb7fdbdbb0dbac"
BASE_URL="https://github.com/mobile-shell/mosh/archive/refs/tags/${MOSH_TAG}.tar.gz"

cache_dir="${CACHE_DIR:-.cache}/sources"
archive="${cache_dir}/${MOSH_TAG}.tar.gz"

mkdir -p "${cache_dir}"

if [ ! -f "${archive}" ]; then
    curl -L -sS -o "${archive}.tmp" "${BASE_URL}"
    mv "${archive}.tmp" "${archive}"
fi

actual="$(shasum -a 256 "${archive}" | awk '{print $1}')"
if [ "${actual}" != "${MOSH_SHA256}" ]; then
    printf '%s\n' "Checksum mismatch for ${archive}" >&2
    printf '%s\n' "expected ${MOSH_SHA256}" >&2
    printf '%s\n' "actual   ${actual}" >&2
    exit 1
fi

printf '%s\n' "${archive}"
