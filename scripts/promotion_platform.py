#!/usr/bin/env python3
"""Validate the native promotion toolchain and describe a cache-safe configuration.

This module never installs tools, substitutes a platform, configures CMake, or
modifies a source/build tree. Call ``probe_environment`` before restoring a
trusted build cache; include its ``environment_fingerprint`` in the cache key.
The caller owns dependency/source integrity and supplies its dependency SHA.
``configure_args`` contains arguments after the resolved ``tools.cmake.path``.
All probes are bounded subprocesses and all configuration is explicit.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
from typing import Mapping


SCHEMA_VERSION = 1
GENERATOR = "Ninja"
BUILD_TYPE = "Release"
# Branch stages are external orchestration; product builds expose one channel.
CHANNELS = ("release",)
# These can affect compiler/linker selection, system headers, or CMake checks.
# Credentials and unrelated GitHub run identifiers must never enter the record.
BUILD_ENVIRONMENT_KEYS = (
    "PATH", "PATHEXT", "CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS",
    "CL", "_CL_", "LINK", "_LINK_", "INCLUDE", "LIB", "LIBPATH",
    "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH",
    "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "SDKROOT", "DEVELOPER_DIR",
    "MACOSX_DEPLOYMENT_TARGET", "VCToolsVersion", "VCToolsInstallDir",
    "WindowsSDKVersion", "WindowsSdkDir", "UCRTVersion", "UniversalCRTSdkDir",
    "VSCMD_ARG_TGT_ARCH", "VSCMD_ARG_HOST_ARCH", "CMAKE_PREFIX_PATH",
    "CMAKE_LIBRARY_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_TOOLCHAIN_FILE",
    "STYIO_LLVM_ROOT", "STYIO_LLVM_DIR", "ICU_ROOT", "STYIO_OSX_SYSROOT",
    "STYIO_TEST_BASH", "STYIO_TEST_NATIVE_TMP", "STYIO_NATIVE_CXX", "STYIO_NATIVE_CC",
)


class PlatformError(RuntimeError):
    """The host cannot satisfy the requested, native promotion configuration."""


def detect_platform(expected_platform: str | None = None) -> tuple[str, str]:
    """Return native OS/architecture, rejecting platform substitution."""
    names = {"Linux": "linux", "Darwin": "macos", "Windows": "windows"}
    system = platform.system()
    if system not in names:
        raise PlatformError(f"Unsupported native platform: {system!r}")
    actual = names[system]
    aliases = {"darwin": "macos", "macos": "macos", "linux": "linux", "windows": "windows"}
    if expected_platform is not None:
        requested = aliases.get(expected_platform.lower())
        if requested is None or requested != actual:
            raise PlatformError(f"Requested platform {expected_platform!r} does not match native {actual}")
    machine = platform.machine().lower()
    architectures = {"amd64": "x86_64", "x86_64": "x86_64", "arm64": "aarch64", "aarch64": "aarch64"}
    if machine not in architectures:
        raise PlatformError(f"Unsupported native architecture: {machine!r}")
    return actual, architectures[machine]


def _path(value: str | Path) -> str:
    # Preserve executable symlink names: resolving clang++ to clang changes its
    # driver mode. Absolute paths still bind CMake's cached executable location.
    result = Path(os.path.abspath(os.fspath(value))).as_posix()
    if any(character in result for character in ("\n", "\r", ";")):
        raise PlatformError("Toolchain paths must not contain newlines or CMake list separators")
    return result


def _executable(value: str, env: Mapping[str, str]) -> str:
    found = shutil.which(value, path=env.get("PATH", os.defpath))
    if not found:
        raise PlatformError(f"Required executable was not found: {value}")
    return _path(found)


def _run(command: list[str], env: Mapping[str, str], *, allow_nonzero: bool = False) -> str:
    try:
        completed = subprocess.run(
            command, env=dict(env), text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=20, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise PlatformError(f"Tool probe failed for {command[0]}: {error}") from error
    output = completed.stdout.strip()
    if completed.returncode and not allow_nonzero:
        raise PlatformError(f"Tool probe failed ({completed.returncode}): {' '.join(command)}\n{output}")
    return output


def _version(output: str, expression: str, name: str) -> tuple[str, tuple[int, ...]]:
    match = re.search(expression, output, re.IGNORECASE)
    if not match:
        raise PlatformError(f"Cannot identify {name} version from: {output[:300]!r}")
    version = match.group(1)
    return version, tuple(int(part) for part in version.split("."))


def _tool(name: str, executable: str, env: Mapping[str, str], expression: str) -> dict:
    path = _executable(executable, env)
    banner = _run([path, "--version"], env)
    version, _ = _version(banner, expression, name)
    return {"path": path, "version": version, "banner": banner}


def _compiler(executable: str, env: Mapping[str, str], host: str) -> dict:
    path = _executable(executable, env)
    is_msvc = Path(path).name.lower() in ("cl", "cl.exe")
    # cl emits its version banner to stderr and exits nonzero without a source.
    banner = _run([path] if is_msvc else [path, "--version"], env, allow_nonzero=is_msvc)
    if is_msvc:
        version, numbers = _version(banner, r"Compiler Version\s+(\d+\.\d+(?:\.\d+)+)", "MSVC")
        if host != "windows" or numbers[0] != 19:
            raise PlatformError("Native Windows builds require MSVC 19.x")
        family = "msvc"
    else:
        version, numbers = _version(banner, r"(?<!Apple )clang version\s+(\d+\.\d+\.\d+)", "Clang")
        if "Apple clang" in banner or numbers[0] != 18:
            raise PlatformError(f"Clang 18.x is required; found {version}")
        if host == "windows":
            raise PlatformError("The controlled Windows toolchain uses MSVC cl, not a substituted compiler")
        family = "clang"
    return {"path": path, "version": version, "family": family, "banner": banner}


def _llvm(env: Mapping[str, str], host: str) -> tuple[dict, str, str]:
    root_value = env.get("STYIO_LLVM_ROOT")
    dir_value = env.get("STYIO_LLVM_DIR") or env.get("LLVM_DIR")
    if not root_value:
        if dir_value:
            root_value = str(Path(dir_value).parent.parent.parent)
        elif host == "linux":
            root_value = "/usr/lib/llvm-18"
        else:
            raise PlatformError("Set STYIO_LLVM_ROOT to the native LLVM 18.1.x development installation")
    root = _path(root_value)
    directory = _path(dir_value or Path(root) / "lib" / "cmake" / "llvm")
    config_file = Path(directory) / "LLVMConfig.cmake"
    try:
        config = config_file.read_text(encoding="utf-8")
    except OSError as error:
        raise PlatformError(f"LLVM development configuration is missing: {config_file}") from error
    package_version, _ = _version(
        config, r'set\(\s*LLVM_PACKAGE_VERSION\s+"?(\d+\.\d+\.\d+)"?\s*\)', "LLVM package",
    )
    config_exe = Path(root) / "bin" / ("llvm-config.exe" if host == "windows" else "llvm-config")
    tool = _tool("LLVM", str(config_exe), env, r"^(\d+\.\d+\.\d+)(?:\s|$)")
    if not re.fullmatch(r"18\.1\.\d+", tool["version"]):
        raise PlatformError(f"LLVM 18.1.x is required; found {tool['version']}")
    if package_version != tool["version"]:
        raise PlatformError(f"LLVM package/config executable mismatch: {package_version} vs {tool['version']}")
    tool["config_sha256"] = hashlib.sha256(config.encode("utf-8")).hexdigest()
    return tool, root, directory


def probe_environment(
    source_root: str | Path,
    build_root: str | Path,
    channel: str,
    dependency_sha: str,
    *,
    expected_platform: str | None = None,
    environ: Mapping[str, str] | None = None,
) -> dict:
    """Return validated tools, configure arguments, and exact cache identity.

    ``dependency_sha`` is the caller's immutable dependency-lock digest (SHA-1
    or SHA-256), not a branch name. Source identity belongs in the caller's cache
    key separately. Changing paths, channel, tool versions, image, SDK, or any
    tracked build environment variable invalidates this fingerprint. The record
    deliberately excludes authentication variables and is safe to log.
    """
    env = dict(os.environ if environ is None else environ)
    host, architecture = detect_platform(expected_platform)
    runner_os = env.get("RUNNER_OS", "").lower()
    if runner_os and runner_os != host:
        raise PlatformError(f"Runner OS {runner_os!r} does not match native {host}")
    runner_arch = env.get("RUNNER_ARCH", "").lower()
    if runner_arch:
        expected_arch = {"x64": "x86_64", "arm64": "aarch64"}.get(runner_arch)
        if expected_arch != architecture:
            raise PlatformError(f"Runner architecture {runner_arch!r} does not match native {architecture}")
    if channel not in CHANNELS:
        raise PlatformError(f"Unsupported promotion channel: {channel!r}")
    if not re.fullmatch(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})", dependency_sha):
        raise PlatformError("dependency_sha must be a complete SHA-1 or SHA-256 digest")
    source, build = _path(source_root), _path(build_root)
    if not (Path(source) / "CMakeLists.txt").is_file():
        raise PlatformError(f"Source root has no CMakeLists.txt: {source}")
    if Path(source).resolve() == Path(build).resolve():
        raise PlatformError("Promotion builds must be out of source")
    if env.get("CMAKE_TOOLCHAIN_FILE"):
        raise PlatformError("CMAKE_TOOLCHAIN_FILE overrides are not part of the controlled native configuration")

    tools = {
        "cmake": _tool("CMake", "cmake", env, r"cmake version\s+(\d+\.\d+\.\d+)"),
        "ctest": _tool("CTest", "ctest", env, r"ctest version\s+(\d+\.\d+\.\d+)"),
        "ninja": _tool("Ninja", "ninja", env, r"^(\d+\.\d+\.\d+)(?=[\s.+-]|$)"),
    }
    if tuple(map(int, tools["cmake"]["version"].split("."))) < (3, 29, 0):
        raise PlatformError("CMake/CTest >= 3.29 is required for --tests-from-file")
    if tools["ctest"]["version"] != tools["cmake"]["version"]:
        raise PlatformError("CMake and CTest must have exactly matching versions")
    if tuple(map(int, tools["ninja"]["version"].split("."))) < (1, 10, 0):
        raise PlatformError("Ninja >= 1.10 is required")
    tools["llvm"], llvm_root, llvm_dir = _llvm(env, host)
    cc = env.get("CC") or ("cl" if host == "windows" else str(Path(llvm_root) / "bin" / "clang"))
    cxx = env.get("CXX") or ("cl" if host == "windows" else str(Path(llvm_root) / "bin" / "clang++"))
    tools["cc"] = _compiler(cc, env, host)
    tools["cxx"] = _compiler(cxx, env, host)
    if (tools["cc"]["family"], tools["cc"]["version"]) != (tools["cxx"]["family"], tools["cxx"]["version"]):
        raise PlatformError("C and C++ compilers must have matching families and exact versions")
    fuzz = {
        "enabled": host != "windows",
        "reason": ("Windows MSVC does not provide the required Clang libFuzzer toolchain"
                   if host == "windows" else "Native Clang platform includes the existing fuzz smoke targets"),
    }
    args = [
        "-S", source, "-B", build, "-G", GENERATOR,
        f"-DCMAKE_BUILD_TYPE={BUILD_TYPE}",
        f"-DCMAKE_MAKE_PROGRAM={tools['ninja']['path']}",
        f"-DCMAKE_C_COMPILER={tools['cc']['path']}",
        f"-DCMAKE_CXX_COMPILER={tools['cxx']['path']}",
        f"-DLLVM_DIR={llvm_dir}",
        f"-DSTYIO_NATIVE_TOOLCHAIN_ROOT={llvm_root}",
        "-DSTYIO_NATIVE_TOOLCHAIN_MODE=auto", "-DSTYIO_INSTALL_NATIVE_TOOLCHAIN=OFF",
        "-DSTYIO_BUILD_NANO=ON", "-DSTYIO_NANO_OPTIMIZE_FOR_SIZE=ON",
        "-DSTYIO_ENABLE_TREE_SITTER=ON", f"-DSTYIO_ENABLE_FUZZ={'ON' if fuzz['enabled'] else 'OFF'}",
        f"-DCMAKE_INSTALL_PREFIX={_path(Path(build) / 'install')}",
    ]
    prefixes = [llvm_root]
    icu_root = env.get("ICU_ROOT")
    icu_identity = None
    if icu_root:
        icu_root = _path(icu_root)
        header = Path(icu_root) / "include" / "unicode" / "uvernum.h"
        try:
            icu_version, _ = _version(header.read_text(encoding="utf-8"), r'#define\s+U_ICU_VERSION\s+"(\d+(?:\.\d+)+)"', "ICU")
        except OSError as error:
            raise PlatformError(f"ICU_ROOT has no ICU development version header: {header}") from error
        prefixes.append(icu_root)
        icu_identity = {"root": icu_root, "version": icu_version}
        args.extend([f"-DICU_ROOT={icu_root}", "-DSTYIO_USE_ICU=ON"])
    else:
        args.append("-DSTYIO_USE_ICU=OFF")
    args.append(f"-DCMAKE_PREFIX_PATH={';'.join(prefixes)}")
    sdk = None
    if host == "macos":
        xcrun = _executable("xcrun", env)
        sdk_path = _path(env.get("STYIO_OSX_SYSROOT") or env.get("SDKROOT") or _run([xcrun, "--sdk", "macosx", "--show-sdk-path"], env))
        if not Path(sdk_path).is_dir():
            raise PlatformError(f"macOS SDK does not exist: {sdk_path}")
        sdk = {"path": sdk_path, "version": _run([xcrun, "--sdk", "macosx", "--show-sdk-version"], env)}
        args.append(f"-DCMAKE_OSX_SYSROOT={sdk_path}")
    if host == "windows":
        # Conda-forge LLVM development packages expose zstd via this location.
        zstd_dir = Path(llvm_root) / "lib" / "cmake" / "zstd"
        if not zstd_dir.is_dir():
            raise PlatformError(f"Windows LLVM dependency zstd CMake directory is missing: {zstd_dir}")
        args.extend([f"-Dzstd_DIR={_path(zstd_dir)}", "-Dgtest_force_shared_crt=ON"])

    identity = {
        "schema_version": SCHEMA_VERSION, "platform": host, "architecture": architecture,
        "os_release": platform.release(), "os_version": platform.version(),
        "runner_image": {name: env.get(name, "") for name in ("ImageOS", "ImageVersion", "RUNNER_OS", "RUNNER_ARCH")},
        "source_root": source, "build_root": build,
        "generator": GENERATOR, "build_type": BUILD_TYPE, "channel": channel,
        "dependency_sha": dependency_sha.lower(), "tools": tools,
        "llvm_root": llvm_root, "llvm_dir": llvm_dir, "icu": icu_identity, "sdk": sdk,
        "fuzz": fuzz,
        "configure_args": args,
        "build_environment": {key: env[key] for key in BUILD_ENVIRONMENT_KEYS if key in env},
    }
    fingerprint = hashlib.sha256(json.dumps(identity, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    return {
        "platform": host, "architecture": architecture, "tools": tools,
        "configure_args": args, "fingerprint_data": identity,
        "environment_fingerprint": fingerprint,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--build-root", required=True)
    parser.add_argument("--channel", default="release", choices=CHANNELS)
    parser.add_argument("--dependency-sha", required=True)
    parser.add_argument("--platform", choices=("linux", "macos", "windows"))
    options = parser.parse_args(argv)
    try:
        result = probe_environment(options.source_root, options.build_root, options.channel,
                                   options.dependency_sha, expected_platform=options.platform)
    except PlatformError as error:
        print(f"promotion toolchain: {error}", file=sys.stderr)
        return 2
    print(json.dumps(result, sort_keys=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
