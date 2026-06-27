# Styio Build And Dev Environment

**Purpose:** Provide the repository-level entry point for preparing environment dependencies on a fresh machine and finding the next subsystem-specific docs.

**Last updated:** 2026-06-26

## Who This Is For

1. Contributors preparing `styio-nightly` dependencies on a fresh Debian/Ubuntu VM or container.
2. Contributors who need the common compiler build, test, and docs-audit commands after bootstrap.
3. IDE/LSP contributors who need the repo-level prerequisites before following `docs/external/for-ide/`.
4. Windows contributors building and testing Styio natively with CMake, Ninja or Visual Studio, Python, and LLVM 18.x.

## Fresh Machine Bootstrap

On a fresh Debian/Ubuntu host, start from the repository root:

```bash
./scripts/bootstrap-dev-env.sh
```

That script installs the common native toolchain used by this repository, including `clang-18`, `lld-18`, `llvm-18-dev`, `cmake`, `ninja`, `python3`, the official Node.js `v24.15.0` LTS binary line, and a local `lit` venv for test tooling.

Bootstrap scope:

1. It prepares system and tool dependencies.
2. It does not configure, build, test, commit, or push the repository.
3. After it finishes, export the printed environment variables before running build commands.

## Standardized Baseline

`styio-nightly` and `styio-spio` share the same standardized native baseline:

1. Development host standard: Debian `13` (`trixie`).
2. Compiler toolchain standard: LLVM / Clang / LLD `18.1.x` via the `clang-18` package line.
3. CMake / CTest standard: `3.31.6`.
4. Validation Python standard: `3.13.5`.
5. Node.js standard for grammar maintenance: `v24.15.0` LTS.
6. Repository compatibility floor: CMake `3.20+` and C++20.
7. CI mirror: GitHub Actions on `ubuntu-24.04`, plus Python `3.13.5` and `cmake==3.31.6` installed before validation steps.

## Required Toolchains

1. LLVM `18.1.x` discoverable by `find_package(LLVM ...)`; `18.1.0` remains the compatibility floor accepted by CMake discovery. On Windows this must be an LLVM development install that provides `LLVMConfig.cmake` and C++ LLVM headers, not only a clang/LLVM executable installer.
2. A C++20 compiler and CMake / CTest `3.31.6` for the standardized local and CI toolchain.
3. Python `3.13.5` for docs and lifecycle scripts in the standardized validation pipeline.
4. Node.js `v24.15.0` LTS when regenerating the Tree-sitter grammar.
5. Optional ICU development headers when building with `-DSTYIO_USE_ICU=ON`.

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
ctest --test-dir build/default -L language_feature
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
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
