#!/bin/sh
set -eu

target="${1:-kobo}"
root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
build_dir="${root}/${BUILD_DIR:-build}/${target}"
cache_dir="${root}/${CACHE_DIR:-.cache}"
base_dir="$("${root}/scripts/ensure-koreader-base.sh")"
info_json="${build_dir}/target-info.json"

mkdir -p "${build_dir}/bin" "${build_dir}/debug" "${build_dir}/src"

python3 "${root}/scripts/target-info.py" --target "${target}" --koreader-base "${base_dir}" > "${info_json}"
chost="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["chost"])' "${info_json}")"
cc="${CC:-${chost}-gcc}"
cxx="${CXX:-${chost}-g++}"
strip="${STRIP:-${chost}-strip}"

if ! command -v "${cc}" >/dev/null 2>&1; then
    printf '%s\n' "Missing target compiler: ${cc}" >&2
    printf '%s\n' "Install the KOReader Kobo toolchain or set CC/CXX/STRIP before running make build TARGET=${target}." >&2
    exit 2
fi
if ! command -v "${cxx}" >/dev/null 2>&1; then
    printf '%s\n' "Missing target C++ compiler: ${cxx}" >&2
    exit 2
fi

cflags="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["cflags"])' "${info_json}")"
cxxflags="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["cxxflags"])' "${info_json}")"
ldflags="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["ldflags"])' "${info_json}")"

"${cc}" -Wall -Wextra -Werror -std=c11 ${cflags} \
    "${root}/launcher/mosh.c" -o "${build_dir}/debug/mosh"
cp "${build_dir}/debug/mosh" "${build_dir}/bin/mosh"
if command -v "${strip}" >/dev/null 2>&1; then
    "${strip}" "${build_dir}/bin/mosh"
fi

archive="$(CACHE_DIR="${cache_dir}" "${root}/scripts/fetch-sources.sh")"
src_dir="${build_dir}/src/mosh-mosh-1.4.0"
if [ ! -d "${src_dir}" ]; then
    rm -rf "${src_dir}"
    tar -C "${build_dir}/src" -xf "${archive}"
fi
if [ ! -x "${src_dir}/configure" ]; then
    (cd "${src_dir}" && ./autogen.sh)
fi

mosh_build="${build_dir}/mosh-build"
mosh_install="${build_dir}/mosh-install"
mkdir -p "${mosh_build}" "${mosh_install}"

if [ ! -x "${mosh_install}/bin/mosh-client" ]; then
    cd "${mosh_build}"
    PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR:-${build_dir}/deps/lib/pkgconfig:${base_dir}/build/${chost}/staging/lib/pkgconfig}" \
    PKG_CONFIG_PATH= \
    CC="${cc}" CXX="${cxx}" \
    CFLAGS="${cflags}" CXXFLAGS="${cxxflags}" LDFLAGS="${ldflags} -static-libgcc -static-libstdc++" \
    "${src_dir}/configure" \
        --host="${chost}" \
        --prefix=/ \
        --enable-client \
        --disable-server \
        --disable-examples \
        --disable-completion \
        --disable-ufw \
        --without-utempter \
        --enable-hardening \
        --enable-static-libraries
    make -j"${PARALLEL_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
    make DESTDIR="${mosh_install}" install
fi

cp "${mosh_install}/bin/mosh-client" "${build_dir}/debug/mosh-client"
cp "${mosh_install}/bin/mosh-client" "${build_dir}/bin/mosh-client"
if command -v "${strip}" >/dev/null 2>&1; then
    "${strip}" "${build_dir}/bin/mosh-client"
fi

printf '%s\n' "Built ${build_dir}/bin/mosh and ${build_dir}/bin/mosh-client"
