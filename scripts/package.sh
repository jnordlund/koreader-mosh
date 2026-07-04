#!/bin/sh
set -eu

target="${1:-kobo}"
root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
version="$(sed -n '1p' "${root}/VERSION")"
build_dir="${root}/${BUILD_DIR:-build}/${target}"
dist_dir="${root}/${DIST_DIR:-dist}"
stage="${build_dir}/stage"
plugin_stage="${stage}/plugins/mosh.koplugin"
epoch="${SOURCE_DATE_EPOCH:-1704067200}"

if [ ! -x "${build_dir}/bin/mosh" ] || [ ! -x "${build_dir}/bin/mosh-client" ]; then
    printf '%s\n' "Missing built binaries for ${target}. Run make build TARGET=${target} first." >&2
    exit 2
fi

rm -rf "${stage}"
mkdir -p "${plugin_stage}/bin" "${plugin_stage}/share/terminfo" "${plugin_stage}/share/terminfo-src" "${plugin_stage}/docs" "${plugin_stage}/LICENSES" "${dist_dir}"

cp "${root}/plugin/mosh.koplugin/_meta.lua" "${plugin_stage}/"
cp "${root}/plugin/mosh.koplugin/main.lua" "${plugin_stage}/"
cp "${build_dir}/bin/mosh" "${plugin_stage}/bin/mosh"
cp "${build_dir}/bin/mosh-client" "${plugin_stage}/bin/mosh-client"
cp "${root}/packaging/vt52.terminfo" "${plugin_stage}/share/terminfo-src/vt52.terminfo"
tic -x -o "${plugin_stage}/share/terminfo" "${root}/packaging/vt52.terminfo"
if [ -f "${plugin_stage}/share/terminfo/76/vt52" ] && [ ! -f "${plugin_stage}/share/terminfo/v/vt52" ]; then
    mkdir -p "${plugin_stage}/share/terminfo/v"
    cp "${plugin_stage}/share/terminfo/76/vt52" "${plugin_stage}/share/terminfo/v/vt52"
fi
if [ -f "${plugin_stage}/share/terminfo/v/vt52" ] && [ ! -f "${plugin_stage}/share/terminfo/76/vt52" ]; then
    mkdir -p "${plugin_stage}/share/terminfo/76"
    cp "${plugin_stage}/share/terminfo/v/vt52" "${plugin_stage}/share/terminfo/76/vt52"
fi

[ -f "${root}/README.md" ] && cp "${root}/README.md" "${plugin_stage}/README.md"
[ -f "${root}/COMPATIBILITY.md" ] && cp "${root}/COMPATIBILITY.md" "${plugin_stage}/COMPATIBILITY.md"
[ -f "${root}/OWNER" ] && cp "${root}/OWNER" "${plugin_stage}/OWNER"
[ -f "${root}/VERSION" ] && cp "${root}/VERSION" "${plugin_stage}/VERSION"
for doc in "${root}"/docs/*.md; do
    [ -e "${doc}" ] && cp "${doc}" "${plugin_stage}/docs/"
done
for license in "${root}"/LICENSES/*; do
    [ -e "${license}" ] && cp "${license}" "${plugin_stage}/LICENSES/"
done
cp "${build_dir}/target-info.json" "${plugin_stage}/target-info.json"

manifest="${dist_dir}/koreader-mosh-${version}-${target}.manifest.json"
zipfile="${dist_dir}/koreader-mosh-${version}-${target}.zip"
sha_file="${zipfile}.sha256"

python3 "${root}/scripts/write-manifest.py" \
    --stage "${stage}" \
    --target "${target}" \
    --version "${version}" \
    --output "${manifest}"

python3 - "${stage}" "${epoch}" <<'PY'
import os
import sys
from pathlib import Path

stage = Path(sys.argv[1])
epoch = int(sys.argv[2])
for path in [stage, *stage.rglob("*")]:
    try:
        os.utime(path, (epoch, epoch), follow_symlinks=False)
    except (FileNotFoundError, NotImplementedError):
        pass
PY
rm -f "${zipfile}" "${sha_file}"
(cd "${stage}" && find . -type f -print | LC_ALL=C sort | zip -X -q "../archive.zip" -@)
mv "${build_dir}/archive.zip" "${zipfile}"
shasum -a 256 "${zipfile}" > "${sha_file}"

python3 "${root}/scripts/validate-package.py" --archive "${zipfile}" --manifest "${manifest}" --target "${target}"
printf '%s\n' "${zipfile}"
