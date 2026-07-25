# Styio Build And Dev Environment

**Purpose:** Provide the repository-level entry point for preparing environment dependencies on a fresh machine and finding the next subsystem-specific docs.

**Last updated:** 2026-07-26

## Who This Is For

1. Contributors preparing `styio-nightly` dependencies on a fresh Debian/Ubuntu VM or container.
2. Contributors who need the common compiler build, test, and docs-audit commands after bootstrap.
3. IDE/LSP contributors who need the repo-level prerequisites before following `docs/external/for-ide/`.
4. Windows contributors building and testing Styio natively with CMake, Ninja or Visual Studio, Python, and a supported LLVM development package.

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
2. LLVM backend floor: LLVM `18.1.0`; each host may use any newer compatible LLVM development package.
3. CMake / CTest standard: `3.31.6`.
4. Validation Python standard: `3.13.5`.
5. Node.js standard for grammar maintenance: `v24.15.0` LTS.
6. Repository compatibility floor: CMake `3.20+` and C++20.
7. CI mirror: GitHub Actions on `ubuntu-24.04`, plus Python `3.13.5` and `cmake==3.31.6` installed before validation steps.

## Required Toolchains

1. LLVM `18.1.0` or newer discoverable by `find_package(LLVM ...)`. On Windows this must be an LLVM development install that provides `LLVMConfig.cmake` and C++ LLVM headers, not only a clang/LLVM executable installer.
2. A C++20 compiler and CMake / CTest `3.31.6` for the standardized local and CI toolchain.
3. Python `3.13.5` for docs and lifecycle scripts in the standardized validation pipeline.
4. Node.js `v24.15.0` LTS when regenerating the Tree-sitter grammar.
5. Optional ICU development headers when building with `-DSTYIO_USE_ICU=ON`.

## Native Windows Build

Styio supports native Windows configure, build, and CTest runs without WSL, Cygwin, MSYS, or Git Bash as a runtime/test requirement. Install:

1. An LLVM `18.1.0` or newer development package with `LLVMConfig.cmake` and C++ LLVM headers. The upstream Windows executable installer is compiler-toolchain focused and may not provide enough files for `find_package(LLVM)`.
2. CMake and either Ninja or Visual Studio Build Tools.
3. Python `3.13.x` for repository validation helpers.

The canonical Styio Windows strategy is:

1. Visual Studio 2022 MSVC x64 compiles and links the C++ host binaries.
2. The conda-forge Windows `llvmdev` package supplies a compatible LLVM
   distribution with headers, libraries, tools, and `LLVMConfig.cmake`.
3. Matching development packages for zlib, zstd, and libxml2 satisfy the
   imported LLVM CMake targets on Windows.
4. `clang-tools` is installed for LLVM tooling support; it does not replace
   MSVC as the default Styio host compiler.
5. The toolchain is user-local and requires no administrator privileges.

Prepare or repair the canonical environment once:

```powershell
.\scripts\bootstrap-dev-env.ps1
```

The bootstrap verifies the download checksum, installs native packages,
verifies the LLVM `18.1.0` minimum, and persists `LLVM_DIR` plus
`STYIO_NATIVE_TOOLCHAIN_ROOT` for future PowerShell, Codex, and CMake
processes. Re-running it is an idempotent repair/update operation.
Processes that were already open before the first bootstrap must be restarted
once so they inherit the persisted variables.

Configure, build, and test through the shared MSVC preset:

```powershell
cmake --preset windows-msvc-llvm
cmake --build --preset windows-msvc-llvm-debug
ctest --preset windows-msvc-llvm-debug
```

For targeted work, pass targets to the build preset and filters to CTest:

```powershell
cmake --build --preset windows-msvc-llvm-debug --target styio_newparser_internal_test
ctest --preset windows-msvc-llvm-debug -R StyioNewParserInternal
```

Direct `clang-cl`, `clang`, or `clang++` host builds remain optional
compatibility routes. They are not the default Windows development strategy.

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
