#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL_VENV="${STYIO_NIGHTLY_TOOL_VENV:-$HOME/.local/venvs/styio-nightly-tools}"
DEBIAN_STANDARD_VERSION="${STYIO_TOOLCHAIN_DEBIAN_STANDARD_VERSION:-13}"
LLVM_STANDARD_SERIES="${STYIO_TOOLCHAIN_LLVM_STANDARD_SERIES:-18.1.x}"
CMAKE_STANDARD_VERSION="${STYIO_TOOLCHAIN_CMAKE_STANDARD_VERSION:-3.31.6}"
PYTHON_STANDARD_VERSION="${STYIO_TOOLCHAIN_PYTHON_STANDARD_VERSION:-$(tr -d '[:space:]' < "$ROOT/.python-version")}"
NODE_STANDARD_VERSION="${STYIO_TOOLCHAIN_NODE_STANDARD_VERSION:-$(tr -d '[:space:]' < "$ROOT/.nvmrc")}"
LIT_STANDARD_VERSION="${STYIO_TOOLCHAIN_LIT_STANDARD_VERSION:-18.1.8}"
NODE_INSTALL_ROOT="${STYIO_NIGHTLY_NODE_INSTALL_ROOT:-/usr/local/lib/nodejs}"

usage() {
  cat <<EOF
Usage: $(basename "$0")

Prepare the Debian/Ubuntu environment dependencies required to build, test,
and maintain styio-nightly on a fresh Linux container or VM.

This script installs dependencies only. It does not configure, build, test,
commit, or push the repository.

Optional environment:
  STYIO_NIGHTLY_TOOL_VENV   Python virtualenv used for lit
                            Default: $TOOL_VENV

Standardized baseline shared with styio-spio:
  Debian                  $DEBIAN_STANDARD_VERSION (trixie)
  LLVM / Clang / LLD      $LLVM_STANDARD_SERIES via clang-18 toolchain packages
  clang-format            clang-format-18
  CMake / CTest           $CMAKE_STANDARD_VERSION (installed into the tool venv)
  Python                  $PYTHON_STANDARD_VERSION
  Node.js                 v$NODE_STANDARD_VERSION (official LTS tarball)
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

report_standard_baseline() {
  # shellcheck disable=SC1091
  . /etc/os-release
  if [[ "${ID:-}" == "debian" && "${VERSION_ID:-}" == "$DEBIAN_STANDARD_VERSION" ]]; then
    log "host matches the standardized dev baseline: Debian $DEBIAN_STANDARD_VERSION"
    return
  fi

  log "host is ${PRETTY_NAME:-unknown}; standardized dev baseline is Debian $DEBIAN_STANDARD_VERSION (trixie). Continuing with the compatible Debian/Ubuntu bootstrap path."
}

install_system_packages() {
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

install_node() {
  local arch version archive url workdir

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
  workdir="$(mktemp -d)"
  trap 'rm -rf "$workdir"' RETURN

  log "installing official Node.js v$NODE_STANDARD_VERSION into $NODE_INSTALL_ROOT"
  wget -qO "$workdir/$archive" "$url"
  as_root mkdir -p "$NODE_INSTALL_ROOT"
  as_root rm -rf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}"
  as_root tar -xJf "$workdir/$archive" -C "$NODE_INSTALL_ROOT"
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/node" /usr/local/bin/node
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/npm" /usr/local/bin/npm
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/npx" /usr/local/bin/npx
  as_root ln -sf "$NODE_INSTALL_ROOT/node-v${NODE_STANDARD_VERSION}-linux-${arch}/bin/corepack" /usr/local/bin/corepack
}

install_lit() {
  log "installing standardized CMake/CTest and lit into $TOOL_VENV"
  python3 -m venv "$TOOL_VENV"
  "$TOOL_VENV/bin/python" -m pip install --upgrade pip
  "$TOOL_VENV/bin/python" -m pip install \
    "cmake==$CMAKE_STANDARD_VERSION" \
    "lit==$LIT_STANDARD_VERSION"
}

print_summary() {
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

main() {
  if [[ "${1:-}" == "--help" ]]; then
    usage
    exit 0
  fi

  ensure_debian_like
  report_standard_baseline
  install_system_packages
  install_node
  install_lit
  print_summary
}

main "$@"
