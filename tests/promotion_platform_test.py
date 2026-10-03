#!/usr/bin/env python3
"""Offline stdlib tests for the promotion native toolchain contract."""
import contextlib
import io
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import promotion_platform as subject


class PlatformTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        (self.source / "CMakeLists.txt").write_text("project(styio)\n")
        self.llvm = self.root / "llvm"
        self.llvm_dir = self.llvm / "lib" / "cmake" / "llvm"
        self.llvm_dir.mkdir(parents=True)
        (self.llvm_dir / "LLVMConfig.cmake").write_text('set(LLVM_PACKAGE_VERSION "18.1.8")\n')
        (self.llvm / "lib" / "cmake" / "zstd").mkdir()
        self.sdk = self.root / "SDK"
        self.sdk.mkdir()
        self.env = {"PATH": "/controlled/bin", "STYIO_LLVM_ROOT": str(self.llvm),
                    "ImageOS": "ubuntu24", "ImageVersion": "20261001.1"}
        self.versions = {"cmake": "cmake version 3.31.6", "ctest": "ctest version 3.31.6",
                         "ninja": "1.12.1", "llvm-config": "18.1.8", "llvm-config.exe": "18.1.8",
                         "clang": "clang version 18.1.8\nTarget: x86_64-unknown-linux-gnu",
                         "clang++": "clang version 18.1.8\nTarget: x86_64-unknown-linux-gnu",
                         "cl": "Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35217 for x64"}
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.system = self.stack.enter_context(mock.patch.object(subject.platform, "system", return_value="Linux"))
        self.machine = self.stack.enter_context(mock.patch.object(subject.platform, "machine", return_value="x86_64"))
        self.stack.enter_context(mock.patch.object(subject.platform, "release", return_value="host-release"))
        self.stack.enter_context(mock.patch.object(subject.platform, "version", return_value="host-version"))
        self.which = self.stack.enter_context(mock.patch.object(subject.shutil, "which", side_effect=lambda value, path: value if Path(value).is_absolute() else "/controlled/bin/" + value))
        self.run = self.stack.enter_context(mock.patch.object(subject.subprocess, "run", side_effect=self.fake_run))

    def fake_run(self, command, **kwargs):
        name = Path(command[0]).name
        if name == "xcrun":
            output = str(self.sdk) if command[-1] == "--show-sdk-path" else "15.5"
        else:
            output = self.versions[name]
        return subprocess.CompletedProcess(command, 2 if name == "cl" else 0, output)

    def probe(self, **overrides):
        arguments = dict(source_root=self.source, build_root=self.root / "build",
                         channel="release", dependency_sha="a" * 64, environ=self.env)
        arguments.update(overrides)
        return subject.probe_environment(**arguments)

    def test_linux_native_release_ninja_and_compiler_driver_names(self):
        result = self.probe(expected_platform="linux")
        self.assertEqual(result["platform"], "linux")
        args = result["configure_args"]
        self.assertEqual(args[args.index("-G") + 1], "Ninja")
        self.assertIn("-DCMAKE_BUILD_TYPE=Release", args)
        self.assertIn("-DSTYIO_BUILD_NANO=ON", args)
        self.assertIn("-DSTYIO_USE_ICU=OFF", args)
        self.assertIn("-DSTYIO_ENABLE_FUZZ=ON", args)
        self.assertEqual(result["fingerprint_data"]["channel"], "release")
        self.assertTrue(result["tools"]["cxx"]["path"].endswith("clang++"))
        self.assertFalse((self.root / "build").exists())
        self.assertEqual(len(result["environment_fingerprint"]), 64)
        json.dumps(result)
        self.assertTrue(all(call.kwargs["timeout"] == 20 for call in self.run.call_args_list))

    def test_macos_sdk_and_icu(self):
        self.system.return_value = "Darwin"
        self.machine.return_value = "arm64"
        self.env["ImageOS"] = "macos15"
        icu = self.root / "icu"
        (icu / "include" / "unicode").mkdir(parents=True)
        (icu / "include" / "unicode" / "uvernum.h").write_text('#define U_ICU_VERSION "78.1"\n')
        self.env["ICU_ROOT"] = str(icu)
        result = self.probe(expected_platform="macos", channel="release")
        self.assertEqual(result["architecture"], "aarch64")
        self.assertIn(f"-DCMAKE_OSX_SYSROOT={self.sdk.as_posix()}", result["configure_args"])
        self.assertIn("-DSTYIO_USE_ICU=ON", result["configure_args"])
        self.assertEqual(result["fingerprint_data"]["icu"]["version"], "78.1")
        self.assertIn("-DSTYIO_ENABLE_FUZZ=ON", result["configure_args"])

    def test_windows_native_msvc_and_shared_crt(self):
        self.system.return_value = "Windows"
        self.machine.return_value = "AMD64"
        self.env["VCToolsVersion"] = "14.44.35207"
        result = self.probe(expected_platform="windows", channel="release")
        self.assertEqual(result["tools"]["cc"]["family"], "msvc")
        self.assertIn("-Dgtest_force_shared_crt=ON", result["configure_args"])
        self.assertIn("-DSTYIO_ENABLE_FUZZ=OFF", result["configure_args"])
        self.assertIn("MSVC", result["fingerprint_data"]["fuzz"]["reason"])
        self.assertTrue(any(arg.startswith("-Dzstd_DIR=") for arg in result["configure_args"]))

    def test_platform_substitution_and_unknown_hosts_fail(self):
        with self.assertRaisesRegex(subject.PlatformError, "does not match"):
            self.probe(expected_platform="windows")
        self.system.return_value = "MSYS_NT-10.0"
        with self.assertRaisesRegex(subject.PlatformError, "Unsupported native platform"):
            self.probe()
        self.system.return_value = "Linux"
        self.machine.return_value = "i686"
        with self.assertRaisesRegex(subject.PlatformError, "architecture"):
            self.probe()

    def test_ctest_and_cmake_must_match_exactly(self):
        self.versions["ctest"] = "ctest version 3.31.5"
        with self.assertRaisesRegex(subject.PlatformError, "matching versions"):
            self.probe()

    def test_ninja_vendor_suffix_is_accepted_and_fingerprinted(self):
        original = self.probe()["environment_fingerprint"]
        self.versions["ninja"] = "1.13.0.git.kitware.jobserver-pipe-1"
        result = self.probe()
        self.assertEqual(result["tools"]["ninja"]["version"], "1.13.0")
        self.assertEqual(result["tools"]["ninja"]["banner"], self.versions["ninja"])
        self.assertNotEqual(original, result["environment_fingerprint"])

    def test_old_cmake_rejected(self):
        self.versions.update(cmake="cmake version 3.28.6", ctest="ctest version 3.28.6")
        with self.assertRaisesRegex(subject.PlatformError, "CMake/CTest >= 3.29"):
            self.probe()

    def test_wrong_llvm_and_package_mismatch_fail(self):
        self.versions["llvm-config"] = "19.1.7"
        with self.assertRaisesRegex(subject.PlatformError, "LLVM 18.1.x"):
            self.probe()
        self.versions["llvm-config"] = "18.1.7"
        with self.assertRaisesRegex(subject.PlatformError, "mismatch"):
            self.probe()

    def test_wrong_compiler_rejected(self):
        for banner in ("clang version 19.1.0", "Apple clang version 18.1.8", "gcc (GCC) 14.2.0"):
            with self.subTest(banner=banner):
                self.versions["clang"] = banner
                with self.assertRaises(subject.PlatformError):
                    self.probe()

    def test_clang18_distro_minor_does_not_override_llvm_pin(self):
        self.versions.update(clang="Ubuntu clang version 18.0.0", **{"clang++": "Ubuntu clang version 18.0.0"})
        result = self.probe()
        self.assertEqual(result["tools"]["cc"]["version"], "18.0.0")
        self.assertEqual(result["tools"]["llvm"]["version"], "18.1.8")

    def test_runner_os_and_architecture_cannot_substitute_host(self):
        for override in ({"RUNNER_OS": "Windows"}, {"RUNNER_ARCH": "ARM64"}):
            with self.subTest(override=override):
                with self.assertRaisesRegex(subject.PlatformError, "does not match native"):
                    self.probe(environ={**self.env, **override})
        self.assertEqual(self.probe(environ={**self.env, "RUNNER_OS": "Linux", "RUNNER_ARCH": "X64"})["platform"], "linux")

    def test_compiler_pair_mismatch_rejected(self):
        self.versions["clang++"] = "clang version 18.1.7"
        with self.assertRaisesRegex(subject.PlatformError, "matching families"):
            self.probe()

    def test_missing_and_failing_tools_fail_closed(self):
        self.which.side_effect = None
        self.which.return_value = None
        with self.assertRaisesRegex(subject.PlatformError, "not found"):
            self.probe()
        self.which.side_effect = lambda value, path: value
        self.run.side_effect = subprocess.TimeoutExpired("cmake", 20)
        with self.assertRaisesRegex(subject.PlatformError, "probe failed"):
            self.probe()

    def test_fingerprint_is_deterministic_and_ignores_unrelated_credentials(self):
        original = self.probe()["environment_fingerprint"]
        self.env.update(GITHUB_TOKEN="secret-never-record", GITHUB_RUN_ID="1234")
        result = self.probe()
        self.assertEqual(original, result["environment_fingerprint"])
        self.assertNotIn("secret-never-record", json.dumps(result))
        self.assertEqual(original, self.probe()["environment_fingerprint"])

    def test_cache_identity_changes_for_every_controlled_dimension(self):
        original = self.probe()["environment_fingerprint"]
        for arguments in ({"dependency_sha": "b" * 64},
                          {"build_root": self.root / "different-build"}):
            with self.subTest(arguments=arguments):
                self.assertNotEqual(original, self.probe(**arguments)["environment_fingerprint"])
        for key in ("ImageVersion", "LIB", "CXXFLAGS", "MACOSX_DEPLOYMENT_TARGET"):
            with self.subTest(key=key):
                changed_env = {**self.env, key: "changed"}
                self.assertNotEqual(original, self.probe(environ=changed_env)["environment_fingerprint"])
        self.versions.update(cmake="cmake version 3.31.7", ctest="ctest version 3.31.7")
        self.assertNotEqual(original, self.probe()["environment_fingerprint"])

    def test_source_path_in_identity(self):
        alternate = self.root / "other-source"
        alternate.mkdir()
        (alternate / "CMakeLists.txt").write_text("project(styio)\n")
        self.assertNotEqual(self.probe()["environment_fingerprint"], self.probe(source_root=alternate)["environment_fingerprint"])

    def test_missing_development_package_is_error(self):
        (self.llvm_dir / "LLVMConfig.cmake").unlink()
        with self.assertRaisesRegex(subject.PlatformError, "development configuration is missing"):
            self.probe()

    def test_invalid_configuration_and_dependency_identity_fail(self):
        for arguments in ({"dependency_sha": "nightly"}, {"channel": "dev"}, {"channel": "nightly"}, {"channel": "stable"}, {"build_root": self.source}):
            with self.subTest(arguments=arguments):
                with self.assertRaises(subject.PlatformError):
                    self.probe(**arguments)
        with self.assertRaisesRegex(subject.PlatformError, "CMAKE_TOOLCHAIN_FILE"):
            self.probe(environ={**self.env, "CMAKE_TOOLCHAIN_FILE": "cross.cmake"})

    def test_cli_outputs_json_and_error_exit(self):
        argv = ["--source-root", str(self.source), "--build-root", str(self.root / "build"),
                "--channel", "release", "--dependency-sha", "a" * 40]
        with mock.patch.dict(subject.os.environ, self.env, clear=True), contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(subject.main(argv), 0)
            self.assertEqual(json.loads(output.getvalue())["platform"], "linux")
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(subject.main(argv + ["--platform", "windows"]), 2)


@unittest.skipUnless(os.environ.get("STYIO_TEST_NATIVE_COMPILER"),
                     "Set STYIO_TEST_NATIVE_COMPILER to run native portability integration")
class NativeRuntimeTests(unittest.TestCase):
    """Opt-in actual platform compilation; never substitutes a different OS."""
    def test_relocated_icu_module_uses_standard_cmake_helpers(self):
        repository = Path(__file__).resolve().parents[1]
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake, "Native validation requires CMake")
        with tempfile.TemporaryDirectory(prefix="styio-icu-module-") as directory:
            root = Path(directory)
            headers = root / "icu/include/unicode"
            headers.mkdir(parents=True)
            (headers / "utypes.h").write_text("// configure-only fixture\n")
            (headers / "uvernum.h").write_text('#define U_ICU_VERSION "78.3"\n')
            library = root / "icu/libicuuc.a"
            library.write_bytes(b"")
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\nproject(IcuModuleProbe LANGUAGES NONE)\n"
                f'list(PREPEND CMAKE_MODULE_PATH "{repository.as_posix()}")\n'
                f'set(ICU_INCLUDE_DIR "{headers.parent.as_posix()}" CACHE PATH "")\n'
                f'set(ICU_UC_LIBRARY_RELEASE "{library.as_posix()}" CACHE FILEPATH "")\n'
                "find_package(ICU REQUIRED COMPONENTS uc)\n"
                'if(NOT TARGET ICU::uc)\n  message(FATAL_ERROR "ICU target missing")\nendif()\n')
            completed = subprocess.run([cmake, "-S", str(root), "-B", str(root / "build")],
                                       text=True, capture_output=True, timeout=30)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)

    def test_native_c_cpp_loading_exports_cache_cleanup_and_compile_failure(self):
        repository = Path(__file__).resolve().parents[1]
        compiler = os.environ["STYIO_TEST_NATIVE_COMPILER"]
        self.assertIsNotNone(shutil.which(compiler), "Requested native test compiler is missing")
        with tempfile.TemporaryDirectory(prefix="styio native & test ") as directory:
            root = Path(directory)
            include = root / "StyioNative"
            include.mkdir()
            (include / "NativeToolchainConfig.hpp").write_text(
                '#pragma once\n#define STYIO_NATIVE_TOOLCHAIN_MODE "auto"\n'
                '#define STYIO_NATIVE_TOOLCHAIN_ROOT ""\n'
                '#define STYIO_NATIVE_TOOLCHAIN_RELATIVE_DIR "native-toolchain"\n')
            harness = root / "probe.cpp"
            harness.write_text(NATIVE_RUNTIME_PROBE)
            executable = root / ("probe.exe" if os.name == "nt" else "probe")
            implementation = repository / "src" / "StyioNative" / "NativeInterop.cpp"
            if Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe"):
                command = [compiler, "/nologo", "/std:c++20", "/EHsc",
                           "/I" + str(repository / "src"), "/I" + str(root),
                           str(implementation), str(harness), "/Fe" + str(executable)]
            else:
                command = [compiler, "-std=c++20", "-I" + str(repository / "src"),
                           "-I" + str(root), str(implementation), str(harness), "-o", str(executable)]
                if sys.platform.startswith("linux"):
                    command.append("-ldl")
            built = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            temporary = root / "native temporary paths with spaces"
            temporary.mkdir()
            environment = {**os.environ, "TMPDIR": str(temporary), "TMP": str(temporary),
                           "TEMP": str(temporary), "STYIO_NATIVE_CACHE": "0"}
            if environment.get("STYIO_LLVM_ROOT"):
                environment["STYIO_NATIVE_TOOLCHAIN_ROOT"] = environment["STYIO_LLVM_ROOT"]
            def run_probe():
                result = subprocess.run([str(executable)], cwd=root, env=environment,
                                        capture_output=True, text=True, timeout=120)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("failure propagation passed", result.stdout)
                self.assertEqual(list(temporary.iterdir()), [], "Native temporary files leaked")
            run_probe()
            environment.update(STYIO_NATIVE_CACHE="1", STYIO_NATIVE_CACHE_DIR=str(root / "cache space"))
            run_probe()
            run_probe()
            suffix = ".dll" if os.name == "nt" else ".dylib" if sys.platform == "darwin" else ".so"
            self.assertGreaterEqual(len(list((root / "cache space" / "v1").glob("*" + suffix))), 2)


NATIVE_RUNTIME_PROBE = r'''
#include "StyioNative/NativeInterop.hpp"
#include <iostream>
#include <stdexcept>
int main() {
  const std::string body = "long long promote_add(long long a, long long b) { return a + b; } "
                           "long long promote_sub(long long a, long long b) { return a - b; }";
  auto c = styio::native::compile_and_load_block("c", body, {"promote_add"});
  auto add = reinterpret_cast<long long (*)(long long, long long)>(c.symbols.at(0).address);
  if (add(19, 23) != 42) return 2;
  auto cpp = styio::native::compile_and_load_block("c++", "long long unselected_cpp_helper(long long a) { return 2 * a; } "
                                                    "extern \"C\" long long promote_double(long long a) { return unselected_cpp_helper(a); }", {"promote_double"});
  auto twice = reinterpret_cast<long long (*)(long long)>(cpp.symbols.at(0).address);
  if (twice(21) != 42) return 3;
  auto cached = styio::native::compile_and_load_block("c", body, {"promote_add", "promote_add"});
  if (cached.symbols.at(0).address != c.symbols.at(0).address) return 4;
  auto sub_module = styio::native::compile_and_load_block("c", body, {"promote_sub"});
  auto sub = reinterpret_cast<long long (*)(long long, long long)>(sub_module.symbols.at(0).address);
  if (sub(50, 8) != 42) return 7;
  auto sub_hit = styio::native::compile_and_load_block("c", body, {"promote_sub"});
  if (sub_hit.symbols.at(0).address != sub_module.symbols.at(0).address) return 8;
  auto both = styio::native::compile_and_load_block("c", body, {"promote_add", "promote_sub"});
  auto reordered = styio::native::compile_and_load_block("c", body, {"promote_sub", "promote_add"});
  if (both.symbols.size() != 2 || reordered.symbols.size() != 2) return 9;
  for (size_t index = 0; index < both.symbols.size(); ++index) {
    if (both.symbols.at(index).address != reordered.symbols.at(index).address) return 10;
  }
  bool failed = false;
  try { (void)styio::native::compile_and_load_block("c", "long long promote_bad(long long a) { invalid!; }", {"promote_bad"}); }
  catch (const std::exception&) { failed = true; }
  if (!failed) return 5;
  bool missing_c_abi = false;
  try { (void)styio::native::compile_and_load_block("c++", "long long promote_hidden(long long a) { return a; }", {"promote_hidden"}); }
  catch (const std::exception&) { missing_c_abi = true; }
  if (!missing_c_abi) return 6;
  std::cout << "native C/C++ compilation, exports, loading, cache, and failure propagation passed\n";
}
'''


if __name__ == "__main__":
    unittest.main()
