#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/source-minimal"
BUILD_TYPE="Release"
GENERATOR="Ninja"
CMAKE_BIN="${CMAKE:-cmake}"
CC_BIN="${CC:-}"
CXX_BIN="${CXX:-}"
CLEAN=0

usage() {
  cat <<'EOF'
usage: scripts/source-build-minimal.sh [options]

Configure and build the full `styio` compiler using the official source-build
`minimal` profile. This is the stable compiler-side helper consumed by source-
build tooling such as `spio build`.

options:
  --build-dir <path>     override the build directory (default: build/source-minimal)
  --build-type <type>    CMake build type (default: Release)
  --generator <name>     CMake generator (default: Ninja)
  --cmake <path>         CMake binary to invoke (default: cmake from PATH)
  --cc <path>            explicit C compiler
  --cxx <path>           explicit C++ compiler
  --clean                remove the build directory before configuring
  --help                 show this help text

The helper intentionally builds only the `styio` target and disables
`STYIO_BUILD_NANO` so source-build mode stays focused on the full compiler path.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --generator)
      GENERATOR="$2"
      shift 2
      ;;
    --cmake)
      CMAKE_BIN="$2"
      shift 2
      ;;
    --cc)
      CC_BIN="$2"
      shift 2
      ;;
    --cxx)
      CXX_BIN="$2"
      shift 2
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "[source-build-minimal] unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ ${CLEAN} -eq 1 ]]; then
  rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

configure_args=(
  -S "${ROOT_DIR}"
  -B "${BUILD_DIR}"
  -G "${GENERATOR}"
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  -DSTYIO_BUILD_NANO=OFF
)

if [[ -n "${CC_BIN}" ]]; then
  configure_args+=("-DCMAKE_C_COMPILER=${CC_BIN}")
fi

if [[ -n "${CXX_BIN}" ]]; then
  configure_args+=("-DCMAKE_CXX_COMPILER=${CXX_BIN}")
fi

"${CMAKE_BIN}" "${configure_args[@]}"
"${CMAKE_BIN}" --build "${BUILD_DIR}" --target styio

printf '%s\n' "${BUILD_DIR}/bin/styio"
