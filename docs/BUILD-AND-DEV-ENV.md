# Styio Build And Dev Environment

**Purpose:** Provide the repository-level entry point for preparing environment dependencies on a fresh machine and finding the next subsystem-specific docs.

**Last updated:** 2026-07-28

## Who This Is For

1. Contributors preparing `styio-nightly` dependencies on a fresh Debian/Ubuntu VM, container, or macOS host.
2. Contributors who need the common compiler build, test, and docs-audit commands after bootstrap.
3. IDE/LSP contributors who need the repo-level prerequisites before following `docs/external/for-ide/`.
4. macOS contributors building with Xcode Command Line Tools, Homebrew LLVM 18, CMake, Ninja, and Python.
5. Windows contributors building and testing Styio natively with CMake, Ninja or Visual Studio, Python, and LLVM 18.x.

## Fresh Machine Bootstrap

On a fresh Debian/Ubuntu or macOS host, start from the repository root:

```bash
./scripts/bootstrap-dev-env.sh
```

The script detects the host platform. On Debian/Ubuntu it installs the `clang-18`, `lld-18`, `llvm-18-dev`, CMake, Ninja, Python, ICU, and Node.js dependencies. On macOS it requires Xcode Command Line Tools and Homebrew, then installs `llvm@18`, `cmake`, `ninja`, `python@3.13`, `node@24`, `icu4c@78`, and `pkgconf`. Both paths create the local tool venv containing the pinned CMake/CTest and `lit`.

Preview the selected platform plan without installing packages:

```bash
./scripts/bootstrap-dev-env.sh --print-plan
```

Bootstrap scope:

1. It prepares system and tool dependencies.
2. It does not configure, build, test, commit, or push the repository.
3. After it finishes, export the printed environment variables before running build commands.

## Standardized Baseline

`styio-nightly` and `pafio-nightly` share the same standardized native baseline:

1. Development host standard: Debian `13` (`trixie`).
2. Compiler toolchain standard: LLVM / Clang / LLD `18.1.x` via the `clang-18` package line.
3. CMake / CTest standard: `3.31.6`.
4. Validation Python standard: `3.13.5`.
5. Node.js standard for grammar maintenance: `v24.15.0` LTS.
6. Repository compatibility floor: CMake `3.20+` and C++20.
7. CI mirrors: the full Linux gate runs on `ubuntu-24.04`; the macOS compatibility gate runs on `macos-15` with Homebrew `llvm@18`. Both use the repository's pinned CMake/CTest and Python validation line.

The Homebrew bootstrap tracks the compatible Python `3.13.x` and Node.js
`24.x` formula series; Homebrew may advance their patch releases. The pinned
`3.13.5` and `24.15.0` values remain the standardized validation baseline.

## Required Toolchains

1. LLVM `18.1.x` discoverable by `find_package(LLVM ...)`; `18.1.0` remains the compatibility floor accepted by CMake discovery. On Windows this must be an LLVM development install that provides `LLVMConfig.cmake` and C++ LLVM headers, not only a clang/LLVM executable installer.
2. A C++20 compiler and CMake / CTest `3.31.6` for the standardized local and CI toolchain.
3. Python `3.13.5` for docs and lifecycle scripts in the standardized validation pipeline.
4. Node.js `v24.15.0` LTS when regenerating the Tree-sitter grammar.
5. Optional ICU development headers when building with `-DSTYIO_USE_ICU=ON`.

## Native macOS Build

Install Xcode Command Line Tools and Homebrew before running the repository bootstrap. The Homebrew LLVM formula is keg-only, so resolve its prefix instead of recording an architecture-specific installation path:

```bash
LLVM_PREFIX="$(brew --prefix llvm@18)"
ICU_PREFIX="$(brew --prefix icu4c@78)"

cmake -S . -B build/macos \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER="$LLVM_PREFIX/bin/clang" \
  -DCMAKE_CXX_COMPILER="$LLVM_PREFIX/bin/clang++" \
  -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)" \
  -DLLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm" \
  -DCMAKE_PREFIX_PATH="$LLVM_PREFIX;$ICU_PREFIX" \
  -DICU_ROOT="$ICU_PREFIX" \
  -DSTYIO_ENABLE_TREE_SITTER=ON \
  -DSTYIO_USE_ICU=ON

cmake --build build/macos --parallel \
  --target styio styio_nano styio_lspd styio_test styio_security_test \
           styio_resource_topology_test styio_ide_test \
           styio_algorithm_equivalence_test styio_newparser_internal_test \
           styio_parser_internal_test styio_platform_internal_test \
           styio_native_interop_internal_test
ctest --test-dir build/macos -R '^styio_platform_internal_test$' --output-on-failure --no-tests=error
ctest --test-dir build/macos -R '^native_interop_' --output-on-failure --no-tests=error
ctest --test-dir build/macos -R '^styio_build_native_executable_stdin_echo$' --output-on-failure --no-tests=error
ctest --test-dir build/macos -R '^styio_lspd_stdio_framing$' --output-on-failure --no-tests=error
ctest --test-dir build/macos -L security --output-on-failure --no-tests=error
ctest --test-dir build/macos -L styio_pipeline --output-on-failure --no-tests=error
ctest --test-dir build/macos -L resource_topology --output-on-failure --no-tests=error
ctest --test-dir build/macos -L algorithm_equivalence --output-on-failure --no-tests=error
ctest --test-dir build/macos -L ide --output-on-failure --no-tests=error
```

Set `STYIO_ENABLE_TREE_SITTER=OFF` for an offline compiler/LSP build that uses the repository-local edit-time syntax backend. Native `@extern(c)` and `@extern(c++)` blocks produce `.dylib` modules on macOS. Compiler discovery checks the explicit `STYIO_NATIVE_CC` / `STYIO_NATIVE_CXX` and `STYIO_NATIVE_TOOLCHAIN_ROOT` settings before falling back to the configured or system compiler.

## Native Windows Build

Styio supports native Windows configure, build, and CTest runs without WSL, Cygwin, MSYS, or Git Bash as a runtime/test requirement. Install:

1. An LLVM `18.1.x` development package with `LLVMConfig.cmake` and C++ LLVM headers. The upstream Windows executable installer is compiler-toolchain focused and may not provide enough files for `find_package(LLVM)`.
2. CMake and either Ninja or Visual Studio Build Tools.
3. Python `3.13.x` for repository validation helpers.

One reproducible option is conda-forge's native Windows packages:

```powershell
micromamba create -n styio-llvm18 -c conda-forge `
  cmake ninja llvm=18.1.8 llvmdev=18.1.8 clang-tools=18.1.8
micromamba activate styio-llvm18
```

From a Developer PowerShell for Visual Studio 2022, configure with Ninja:

```powershell
$env:LLVM_DIR = "$env:CONDA_PREFIX\Library\lib\cmake\llvm"
$env:STYIO_NATIVE_TOOLCHAIN_ROOT = "$env:CONDA_PREFIX\Library"

cmake -S . -B build/windows -G Ninja `
  -DCMAKE_C_COMPILER=cl `
  -DCMAKE_CXX_COMPILER=cl `
  -DLLVM_DIR="$env:LLVM_DIR" `
  -DSTYIO_NATIVE_TOOLCHAIN_ROOT="$env:STYIO_NATIVE_TOOLCHAIN_ROOT" `
  -DSTYIO_NATIVE_TOOLCHAIN_MODE=auto

cmake --build build/windows --target styio styio_lspd styio-nano styio_test
ctest --test-dir build/windows --output-on-failure
```

`clang-cl`, `clang`, or `clang++` can also be used as the CMake compiler when the selected LLVM development package and Windows SDK are compatible. Visual Studio generators are supported when `LLVM_DIR` points at the LLVM CMake package:

```powershell
cmake -S . -B build/vs -G "Visual Studio 17 2022" `
  -DLLVM_DIR="$env:LLVM_DIR" `
  -DSTYIO_NATIVE_TOOLCHAIN_ROOT="$env:STYIO_NATIVE_TOOLCHAIN_ROOT"

cmake --build build/vs --config Debug --target styio styio_lspd styio-nano styio_test
ctest --test-dir build/vs -C Debug --output-on-failure
```

Native `@extern(c)` and `@extern(c++)` blocks compile to DLLs on Windows. Compiler discovery checks `STYIO_NATIVE_CC` / `STYIO_NATIVE_CXX`, `STYIO_NATIVE_TOOLCHAIN_ROOT`, bundled install paths, then system `clang`, `clang++`, or `clang-cl` according to `STYIO_NATIVE_TOOLCHAIN_MODE` (`auto`, `bundled`, or `system`). The native cache uses `STYIO_NATIVE_CACHE_DIR` first, then `LOCALAPPDATA`, then `TEMP`.

## Typical Build And Test Commands

Configure:

```bash
cmake -S . -B build/default \
  -DSTYIO_ENABLE_TREE_SITTER=ON \
  -DSTYIO_USE_ICU=OFF
```

Build:

```bash
cmake --build build/default -j4
```

Stable full-compiler source-build helper:

```bash
./scripts/source-build-minimal.sh
```

Run language feature and pipeline tests:

```bash
ctest --test-dir build/default -L language_feature --output-on-failure --no-tests=error
ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error
ctest --test-dir build/default -L security --output-on-failure --no-tests=error
```

Run repo docs validation:

```bash
./scripts/docs-gate.sh
./scripts/delivery-gate.sh --skip-health
```

Run the full checkpoint delivery floor:

```bash
./scripts/delivery-gate.sh
```

## Subsystem-Specific Follow-Ups

1. IDE and LSP targets: [external/for-ide/BUILD.md](./external/for-ide/BUILD.md)
2. IDE integration doc index: [external/for-ide/INDEX.md](./external/for-ide/INDEX.md)
3. Team-owned workflow and delivery rules: [teams/INDEX.md](./teams/INDEX.md)
4. Repository workflow assets: [assets/INDEX.md](./assets/INDEX.md)

## Related Docs

1. Repository docs entry: [README.md](./README.md)
2. Documentation policy: [specs/DOCUMENTATION-POLICY.md](./specs/DOCUMENTATION-POLICY.md)
3. Repository ecosystem map: [specs/REPOSITORY-MAP.md](./specs/REPOSITORY-MAP.md)
4. Agent and contributor rules: [specs/AGENT-SPEC.md](./specs/AGENT-SPEC.md)
