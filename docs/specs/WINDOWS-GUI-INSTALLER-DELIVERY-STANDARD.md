# Windows GUI Installer Delivery Standard

**Purpose:** Define the product delivery standard for Windows end users: Styio must ship as a graphical installer that runs without requiring users to install LLVM, CMake, Ninja, conda, or a compiler toolchain.

**Last updated:** 2026-06-28

## Scope

This standard applies to Windows release artifacts for people who want to install and run the Styio programming language, not to contributors building Styio from source.

The Windows product delivery must separate these audiences:

1. End users install Styio with a graphical installer and run `styio` from a normal terminal.
2. Advanced users may download a portable ZIP with the same runtime payload layout.
3. Contributors who build from source may still need LLVM development files, CMake, Ninja, MSVC, or `clang-cl`; those requirements do not belong in the end-user install path.

## Product Principle

Windows users must not be asked to install LLVM, conda, CMake, Ninja, MSYS2, Visual Studio, or Visual Studio Build Tools before they can run a normal `.styio` program.

The product artifact owns the runtime closure required by `styio.exe`. Source-build requirements are a contributor concern, not a user-facing installation requirement.

MSVC / `clang-cl` support is required for producing Windows builds, but it is not a replacement for Styio's LLVM-backed compiler implementation. The release package must hide that implementation detail from ordinary users.

## Required Release Artifacts

Every Windows release must publish these artifacts:

1. `Styio-<version>-windows-x64-installer.exe`
2. `Styio-<version>-windows-x64-portable.zip`
3. `Styio-<version>-windows-x64-manifest.json`
4. Checksums for every downloadable artifact.

The installer is the primary product artifact. The portable ZIP exists for CI, power users, offline inspection, and emergency fallback.

## GUI Installer Standard

The Windows installer must be a graphical `.exe` installer with a flow comparable to the Python Windows installer.

The first screen must make the common path obvious:

```text
Styio Installer

[Install Now]
[Customize]

[x] Add styio.exe to PATH
[x] Install Styio language server
[ ] Install native C/C++ extern toolchain
[ ] Associate .styio files with Styio
```

Required behavior:

1. The default path must install Styio without opening a terminal.
2. The default path must add `styio.exe` to the user PATH unless the user opts out.
3. The installer must install an uninstaller.
4. The installer must support per-user install without administrator rights.
5. The installer should support all-users install when launched with elevated rights.
6. The installer must present optional components through `Customize`, not through separate downloads required for normal use.
7. The installer must not mention LLVM, CMake, Ninja, or conda as required user setup.

Acceptable installer technologies include Inno Setup, WiX, NSIS, or CPack-backed equivalents, provided the resulting experience meets this standard.

## Installed Layout

The installer and portable ZIP must share the same staged payload layout before installer wrapping:

```text
Styio/
  bin/
    styio.exe
    styio_lspd.exe
    zlib.dll
    zstd.dll
    <other required runtime DLLs>
  share/
    styio/
      library/
      prelude/
  examples/
  docs/
  manifest.json
  LICENSE
  NOTICE
```

`bin/` must contain every non-system DLL needed to start `styio.exe` on a clean supported Windows machine. The package must not rely on dependency DLLs being present in a contributor's local conda, MSYS2, vcpkg, or LLVM installation.

## Runtime Dependencies

The release packaging step must inspect `styio.exe` and `styio_lspd.exe` for runtime DLL dependencies.

The package must include project-owned or redistributable runtime DLLs such as:

1. `zlib.dll`
2. `zstd.dll`
3. Any LLVM runtime DLLs if the build links LLVM dynamically.
4. Any other third-party DLLs required by the chosen LLVM distribution.

The package must handle the Microsoft Visual C++ runtime through one approved strategy:

1. Bundle the legally redistributable VC runtime files next to `styio.exe`.
2. Chain-install the official Microsoft Visual C++ Redistributable.
3. Detect a missing supported VC runtime and show a clear graphical repair/install path.

Silent process failure due to missing DLLs is not acceptable.

## Native Extern Toolchain

Normal `.styio` programs must run without a user-installed C/C++ compiler.

For `@extern(c)` and `@extern(c++)`, the release must use a tiered policy:

1. Basic installer: native extern support may report a clear diagnostic if no C/C++ toolchain is available.
2. Full installer option: `Install native C/C++ extern toolchain` installs a bundled toolchain under `bin/native-toolchain/` or an equivalent private directory.
3. System fallback: if a user already has a compatible `clang`, `clang++`, `clang-cl`, or MSVC toolchain, Styio may discover it according to `STYIO_NATIVE_TOOLCHAIN_MODE`.

The bundled native toolchain must be optional because it increases package size. When installed, it must be selected without requiring the user to edit environment variables.

Relevant existing build knobs:

1. `STYIO_INSTALL_NATIVE_TOOLCHAIN`
2. `STYIO_NATIVE_TOOLCHAIN_ROOT`
3. `STYIO_NATIVE_TOOLCHAIN_BUNDLE_ROOT`
4. `STYIO_NATIVE_TOOLCHAIN_RELATIVE_DIR`
5. `STYIO_NATIVE_TOOLCHAIN_MODE`

## Build And Packaging Boundary

Release engineering may use MSVC, `clang-cl`, conda-forge LLVM development packages, vcpkg, or a custom LLVM build to produce the Windows artifact.

That build environment must not leak into the installed product:

1. The installer must not require conda.
2. The installer must not require MSYS2.
3. The installer must not require `LLVMConfig.cmake`.
4. The installer must not require `zstd_DIR`, `LLVM_DIR`, or `CMAKE_PREFIX_PATH`.
5. The installer must not depend on DLLs outside the installed Styio directory or approved Windows system locations.

When using `clang-cl`, packaging validation must guard against mixed ABI dependency discovery, especially MinGW / MSYS2 headers or libraries being pulled into an MSVC ABI build.

## Acceptance Criteria

A Windows installer release is not acceptable until all of these checks pass on a clean supported Windows VM:

1. Install with default options through the GUI.
2. Open a new PowerShell window and run `styio --version`.
3. Run `styio -f examples/hello_world.styio` and verify the expected output.
4. Run `styio --machine-info=json` and verify valid JSON output.
5. Launch `styio_lspd.exe` far enough to prove required DLLs are present.
6. Uninstall Styio through Windows Apps / installed programs UI.
7. Confirm `styio` is removed from PATH after uninstall when the installer added it.
8. Repeat install into a path containing spaces.
9. Verify the portable ZIP works after extraction without installer side effects.
10. Verify failure messaging for missing optional native extern toolchain is explicit and actionable.

The clean VM must not have LLVM, conda, MSYS2, CMake, Ninja, Visual Studio, or Visual Studio Build Tools preinstalled unless the specific test is validating system-toolchain discovery.

## Release Manifest

Each Windows artifact must include a machine-readable manifest with at least:

```json
{
  "schema_version": 1,
  "product": "styio",
  "version": "<version>",
  "platform": "windows-x64",
  "artifact_kind": "installer",
  "compiler_runtime": "msvc",
  "bundled_components": [],
  "required_system_components": [],
  "entrypoints": ["bin/styio.exe", "bin/styio_lspd.exe"]
}
```

The manifest must distinguish bundled dependencies from required system components so support and installer diagnostics can explain failures without referring to contributor-only build setup.

## Documentation Rules

End-user Windows installation docs must start from the GUI installer path.

They may include portable ZIP instructions after the installer flow, but they must not lead with source-build setup. LLVM development package instructions belong in contributor build documentation only.

Release notes must state whether the package includes:

1. Styio compiler CLI.
2. Styio language server.
3. Standard library and prelude resources.
4. Native extern toolchain.
5. VC runtime handling strategy.

## Non-Goals

This standard does not require:

1. Replacing the LLVM compiler backend with an MSVC-only backend.
2. Making source builds work without LLVM development files.
3. Making `@extern(c/c++)` work without either a bundled or system C/C++ toolchain.
4. Supporting mixed MinGW and MSVC ABI dependency graphs in one release artifact.
