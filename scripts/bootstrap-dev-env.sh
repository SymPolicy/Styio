#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL_VENV="${STYIO_NIGHTLY_TOOL_VENV:-$HOME/.local/venvs/styio-nightly-tools}"
HOST_KERNEL="$(uname -s)"
DEBIAN_STANDARD_VERSION="${STYIO_TOOLCHAIN_DEBIAN_STANDARD_VERSION:-13}"
LLVM_STANDARD_SERIES="${STYIO_TOOLCHAIN_LLVM_STANDARD_SERIES:-18.1.x}"
CMAKE_STANDARD_VERSION="${STYIO_TOOLCHAIN_CMAKE_STANDARD_VERSION:-3.31.6}"
PYTHON_STANDARD_VERSION="${STYIO_TOOLCHAIN_PYTHON_STANDARD_VERSION:-$(tr -d '[:space:]' < "$ROOT/.python-version")}"
NODE_STANDARD_VERSION="${STYIO_TOOLCHAIN_NODE_STANDARD_VERSION:-$(tr -d '[:space:]' < "$ROOT/.nvmrc")}"
LIT_STANDARD_VERSION="${STYIO_TOOLCHAIN_LIT_STANDARD_VERSION:-18.1.8}"
NODE_INSTALL_ROOT="${STYIO_NIGHTLY_NODE_INSTALL_ROOT:-/usr/local/lib/nodejs}"
MACOS_LLVM_FORMULA="${STYIO_MACOS_LLVM_FORMULA:-llvm@18}"
MACOS_ICU_FORMULA="${STYIO_MACOS_ICU_FORMULA:-icu4c@78}"
MACOS_PYTHON_SERIES="${STYIO_MACOS_PYTHON_SERIES:-${PYTHON_STANDARD_VERSION%.*}}"
MACOS_NODE_MAJOR="${STYIO_MACOS_NODE_MAJOR:-${NODE_STANDARD_VERSION%%.*}}"
MACOS_PYTHON_FORMULA="${STYIO_MACOS_PYTHON_FORMULA:-python@$MACOS_PYTHON_SERIES}"
MACOS_NODE_FORMULA="${STYIO_MACOS_NODE_FORMULA:-node@$MACOS_NODE_MAJOR}"

usage() {
  local host_summary compiler_summary format_summary python_summary node_summary
  case "$HOST_KERNEL" in
    Darwin)
      host_summary="macOS with Xcode Command Line Tools and Homebrew"
      compiler_summary="$LLVM_STANDARD_SERIES via Homebrew $MACOS_LLVM_FORMULA"
      format_summary="clang-format from Homebrew $MACOS_LLVM_FORMULA"
      python_summary="$MACOS_PYTHON_SERIES.x via Homebrew $MACOS_PYTHON_FORMULA"
      node_summary="v$MACOS_NODE_MAJOR.x via Homebrew $MACOS_NODE_FORMULA"
      ;;
    Linux)
      host_summary="Debian/Ubuntu Linux"
      compiler_summary="$LLVM_STANDARD_SERIES via clang-18 toolchain packages"
      format_summary="clang-format-18"
      python_summary="$PYTHON_STANDARD_VERSION"
      node_summary="v$NODE_STANDARD_VERSION via the official LTS tarball"
      ;;
    *)
      host_summary="a supported Debian/Ubuntu or macOS host"
      compiler_summary="$LLVM_STANDARD_SERIES"
      format_summary="platform clang-format"
      python_summary="$PYTHON_STANDARD_VERSION compatible series"
      node_summary="v$NODE_STANDARD_VERSION compatible series"
      ;;
  esac

  cat <<EOF
Usage: $(basename "$0") [--help|--print-plan]

Prepare the environment dependencies required to build, test, and maintain
styio-nightly on $host_summary.

This script installs dependencies only. It does not configure, build, test,
commit, or push the repository.

Optional environment:
  STYIO_NIGHTLY_TOOL_VENV   Python virtualenv used for lit
                            Default: $TOOL_VENV

Standardized baseline shared with pafio-nightly:
  Debian                  $DEBIAN_STANDARD_VERSION (trixie)
  LLVM / Clang / LLD      $compiler_summary
  clang-format            $format_summary
  CMake / CTest           $CMAKE_STANDARD_VERSION (installed into the tool venv)
  Python                  $python_summary
  Node.js                 $node_summary

Use --print-plan to inspect the platform dependency plan without installing
or changing packages.
EOF
}

log() {
  printf '[styio-nightly env] %s\n' "$*"
}

fail() {
  printf '[styio-nightly env] %s\n' "$*" >&2
  exit 1
}

as_root() {
  if [[ $EUID -eq 0 ]]; then
    "$@"
    return
  fi

  if command -v sudo >/dev/null 2>&1; then
    sudo "$@"
    return
  fi

  fail "sudo is required to install system packages"
}

ensure_debian_like() {
  if [[ ! -r /etc/os-release ]]; then
    fail "/etc/os-release is missing; only Debian/Ubuntu hosts are supported"
  fi

  # shellcheck disable=SC1091
  . /etc/os-release

  local family="${ID_LIKE:-}"
  if [[ "${ID:-}" != "debian" && "${ID:-}" != "ubuntu" && "${family}" != *debian* && "${family}" != *ubuntu* ]]; then
    fail "unsupported distribution: ${PRETTY_NAME:-unknown}. Expected Debian/Ubuntu."
  fi
}

report_debian_standard_baseline() {
  # shellcheck disable=SC1091
  . /etc/os-release
  if [[ "${ID:-}" == "debian" && "${VERSION_ID:-}" == "$DEBIAN_STANDARD_VERSION" ]]; then
    log "host matches the standardized dev baseline: Debian $DEBIAN_STANDARD_VERSION"
    return
  fi

  log "host is ${PRETTY_NAME:-unknown}; standardized dev baseline is Debian $DEBIAN_STANDARD_VERSION (trixie). Continuing with the compatible Debian/Ubuntu bootstrap path."
}

install_debian_system_packages() {
  local packages=(
    build-essential
    ca-certificates
    clang-18
    clang-format-18
    cmake
    curl
    git
    libcurl4-openssl-dev
    libedit-dev
    libffi-dev
    libicu-dev
    libssl-dev
    libxml2-dev
    libzstd-dev
    lld-18
    llvm-18-dev
    llvm-18-tools
    ninja-build
    pkg-config
    python3
    python3-pip
    python3-venv
    rsync
    unzip
    wget
    zip
  )

  log "installing system packages"
  as_root apt-get update
  as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${packages[@]}"
  if [[ -x /usr/bin/clang-format-18 ]]; then
    as_root ln -sf /usr/bin/clang-format-18 /usr/local/bin/clang-format
  fi
}

node_arch() {
  case "$(uname -m)" in
    x86_64|amd64)
      echo "x64"
      ;;
    aarch64|arm64)
      echo "arm64"
      ;;
    *)
      fail "unsupported architecture for official Node.js binaries: $(uname -m)"
      ;;
  esac
}

install_debian_node() {
  local arch version archive url checksum_url workdir

  if command -v node >/dev/null 2>&1; then
    version="$(node --version 2>/dev/null || true)"
    if [[ "$version" == "v$NODE_STANDARD_VERSION" ]]; then
      log "Node.js already matches standardized version $version"
      return
    fi
  fi

  arch="$(node_arch)"
  archive="node-v${NODE_STANDARD_VERSION}-linux-${arch}.tar.xz"
  url="https://nodejs.org/dist/v${NODE_STANDARD_VERSION}/${archive}"
  checksum_url="https://nodejs.org/dist/v${NODE_STANDARD_VERSION}/SHASUMS256.txt"
  workdir="$(mktemp -d)"
  trap 'rm -rf "$workdir"' RETURN

  log "installing official Node.js v$NODE_STANDARD_VERSION into $NODE_INSTALL_ROOT"
  wget -qO "$workdir/$archive" "$url"
  wget -qO "$workdir/SHASUMS256.txt" "$checksum_url"
  (cd "$workdir" && grep -F "  $archive" SHASUMS256.txt | sha256sum -c -)
  as_root mkdir -p "$NODE_INSTALL_ROOT"
  as_root rm -rf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}"
  as_root tar -xJf "$workdir/$archive" -C "$NODE_INSTALL_ROOT"
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/node" /usr/local/bin/node
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/npm" /usr/local/bin/npm
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/npx" /usr/local/bin/npx
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/corepack" /usr/local/bin/corepack
}

install_lit() {
  local python_bin="${1:-python3}"
  log "installing standardized CMake/CTest and lit into $TOOL_VENV"
  "$python_bin" -m venv "$TOOL_VENV"
  "$TOOL_VENV/bin/python" -m pip install --upgrade pip
  "$TOOL_VENV/bin/python" -m pip install \
    "cmake==$CMAKE_STANDARD_VERSION" \
    "lit==$LIT_STANDARD_VERSION"
}

ensure_macos() {
  if ! command -v xcrun >/dev/null 2>&1 || ! xcrun --show-sdk-path >/dev/null 2>&1; then
    fail "Xcode Command Line Tools are required on macOS; install them with xcode-select --install"
  fi
  if ! command -v brew >/dev/null 2>&1; then
    fail "Homebrew is required on macOS; install it from https://brew.sh before rerunning bootstrap"
  fi
}

install_macos_packages() {
  local packages=(
    "$MACOS_LLVM_FORMULA"
    "$MACOS_ICU_FORMULA"
    "$MACOS_PYTHON_FORMULA"
    "$MACOS_NODE_FORMULA"
    cmake
    ninja
    pkgconf
  )

  log "installing macOS dependencies with Homebrew"
  brew install "${packages[@]}"
}

macos_formula_prefix() {
  brew --prefix "$1"
}

validate_macos_packages() {
  local llvm_prefix python_prefix node_prefix python_bin llvm_version python_version node_version
  llvm_prefix="$(macos_formula_prefix "$MACOS_LLVM_FORMULA")"
  python_prefix="$(macos_formula_prefix "$MACOS_PYTHON_FORMULA")"
  node_prefix="$(macos_formula_prefix "$MACOS_NODE_FORMULA")"
  python_bin="$python_prefix/bin/python$MACOS_PYTHON_SERIES"

  llvm_version="$("$llvm_prefix/bin/llvm-config" --version 2>/dev/null || true)"
  python_version="$("$python_bin" --version 2>&1 || true)"
  node_version="$("$node_prefix/bin/node" --version 2>/dev/null || true)"

  [[ "$llvm_version" == 18.1.* ]] \
    || fail "Homebrew $MACOS_LLVM_FORMULA must provide LLVM 18.1.x; found ${llvm_version:-unknown}"
  [[ "$python_version" == "Python $MACOS_PYTHON_SERIES."* ]] \
    || fail "Homebrew $MACOS_PYTHON_FORMULA must provide Python $MACOS_PYTHON_SERIES.x; found ${python_version:-unknown}"
  [[ "$node_version" == "v$MACOS_NODE_MAJOR."* ]] \
    || fail "Homebrew $MACOS_NODE_FORMULA must provide Node.js $MACOS_NODE_MAJOR.x; found ${node_version:-unknown}"
}

print_plan() {
  case "$HOST_KERNEL" in
    Darwin)
      cat <<EOF
Host: macOS
Package manager: Homebrew
Packages: $MACOS_LLVM_FORMULA $MACOS_ICU_FORMULA $MACOS_PYTHON_FORMULA $MACOS_NODE_FORMULA cmake ninja pkgconf
Python tools: cmake==$CMAKE_STANDARD_VERSION lit==$LIT_STANDARD_VERSION
Python runtime: $MACOS_PYTHON_SERIES.x compatible formula series
Node.js runtime: v$MACOS_NODE_MAJOR.x compatible formula series
EOF
      ;;
    Linux)
      cat <<EOF
Host: Debian/Ubuntu
Package manager: apt
Packages: clang-18 clang-format-18 lld-18 llvm-18-dev llvm-18-tools cmake ninja-build python3 python3-venv libicu-dev
Python tools: cmake==$CMAKE_STANDARD_VERSION lit==$LIT_STANDARD_VERSION
Node.js: v$NODE_STANDARD_VERSION official Linux archive
EOF
      ;;
    *)
      fail "unsupported host kernel: $HOST_KERNEL"
      ;;
  esac
}

print_debian_summary() {
  cat <<EOF

styio-nightly environment dependencies are ready.

Standardized baseline:
  Debian:        $DEBIAN_STANDARD_VERSION (trixie)
  LLVM series:   $LLVM_STANDARD_SERIES
  clang-format:  clang-format-18
  CMake/CTest:   $CMAKE_STANDARD_VERSION
  Python:        $PYTHON_STANDARD_VERSION
  Node.js:       v$NODE_STANDARD_VERSION

Suggested shell exports:
  export CC=/usr/bin/clang-18
  export CXX=/usr/bin/clang++-18
  export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
  export PATH="$TOOL_VENV/bin:\$PATH"

Build and test steps are intentionally separate. Typical next steps:
  cmake -S "$ROOT" -B "$ROOT/build"
  cmake --build "$ROOT/build" -j"$(nproc)"
EOF
}

print_macos_summary() {
  local llvm_prefix icu_prefix node_prefix python_prefix jobs
  llvm_prefix="$(macos_formula_prefix "$MACOS_LLVM_FORMULA")"
  icu_prefix="$(macos_formula_prefix "$MACOS_ICU_FORMULA")"
  node_prefix="$(macos_formula_prefix "$MACOS_NODE_FORMULA")"
  python_prefix="$(macos_formula_prefix "$MACOS_PYTHON_FORMULA")"
  jobs="$(sysctl -n hw.logicalcpu 2>/dev/null || sysctl -n hw.ncpu)"

  cat <<EOF

styio-nightly environment dependencies are ready.

macOS compatibility mirror:
  LLVM series:   $LLVM_STANDARD_SERIES via $MACOS_LLVM_FORMULA
  CMake/CTest:   $CMAKE_STANDARD_VERSION in the tool venv
  Python:        $MACOS_PYTHON_SERIES.x via $MACOS_PYTHON_FORMULA
  Node.js:       v$MACOS_NODE_MAJOR.x via $MACOS_NODE_FORMULA
  ICU:           $MACOS_ICU_FORMULA

Suggested shell exports:
  export CC="$llvm_prefix/bin/clang"
  export CXX="$llvm_prefix/bin/clang++"
  export LLVM_DIR="$llvm_prefix/lib/cmake/llvm"
  export CMAKE_PREFIX_PATH="$llvm_prefix;$icu_prefix"
  export PATH="$TOOL_VENV/bin:$llvm_prefix/bin:$python_prefix/bin:$node_prefix/bin:\$PATH"

Build and test steps are intentionally separate. Typical next steps:
  cmake -S "$ROOT" -B "$ROOT/build/macos" -DCMAKE_OSX_SYSROOT="\$(xcrun --show-sdk-path)"
  cmake --build "$ROOT/build/macos" -j"$jobs"
EOF
}

main() {
  if [[ "${1:-}" == "--help" ]]; then
    usage
    exit 0
  fi
  if [[ "${1:-}" == "--print-plan" ]]; then
    print_plan
    exit 0
  fi
  if [[ $# -ne 0 ]]; then
    usage >&2
    exit 2
  fi

  case "$HOST_KERNEL" in
    Darwin)
      ensure_macos
      install_macos_packages
      validate_macos_packages
      install_lit "$(macos_formula_prefix "$MACOS_PYTHON_FORMULA")/bin/python$MACOS_PYTHON_SERIES"
      print_macos_summary
      ;;
    Linux)
      ensure_debian_like
      report_debian_standard_baseline
      install_debian_system_packages
      install_debian_node
      install_lit
      print_debian_summary
      ;;
    *)
      fail "unsupported host kernel: $HOST_KERNEL"
      ;;
  esac
}

main "$@"
