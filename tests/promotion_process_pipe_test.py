#!/usr/bin/env python3
"""Real subprocess regressions for StyioUtil/ProcessPipe.hpp.

Set STYIO_TEST_NATIVE_COMPILER to the native C++ compiler executable. CTest sets
it explicitly; an ordinary offline unittest invocation may omit it and skip the
compile/run integration. Windows additionally requires STYIO_TEST_BASH to name
an absolute, verified Git-for-Windows bash.exe. Missing or broken configuration
is an error when a native compiler was requested, never an integration skip.

No LLVM, GoogleTest, downloads, or repository build directory is needed.
"""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
NATIVE_COMPILER = os.environ.get("STYIO_TEST_NATIVE_COMPILER")
LITERAL = 'spaces \' quote " double $HOME `echo unsafe` & ; | %PATH% ! trailing \\'
PATH_LITERAL = "spaces ' dollar $HOME `echo unsafe` & semicolon ; %PATH%"
ARGV_FIXTURES = ["", "one two", "single'quote", 'double"quote', "trailing\\",
                 'backslash\\"quote', "$HOME `echo unsafe` & ; %PATH% !",
                 "C:\\path with spaces\\file.txt"]

HARNESS = r'''
#include "StyioUtil/ProcessPipe.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <shellapi.h>
#endif

namespace fs = std::filesystem;
using styio::util::capture_shell_stdout;
using styio::util::shell_path;
using styio::util::shell_path_contents;
using styio::util::shell_quote;

std::string hex(const std::string& value) {
  static const char digits[] = "0123456789abcdef";
  std::string result;
  for (unsigned char ch : value) {
    result += digits[ch >> 4];
    result += digits[ch & 15];
  }
  return result;
}

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void emit(const styio::util::ProcessCapture& capture) {
  std::cout << capture.exit_code << '\n' << capture.raw_status << '\n'
            << hex(capture.stdout_text) << '\n';
}

#ifdef _WIN32
void verify_windows_quoting() {
  const std::vector<std::wstring> values = {
    L"", L"one two", L"C:\\Program Files\\Git\\bin\\bash.exe",
    L"a\\\"b", L"a\\", L"\"", L"$foo %PATH% ' ; &", L"Unicode \u00e9",
    L"line\nbreak\ttab"
  };
  for (const auto& value : values) {
    const auto command = L"fixture " + styio::util::process_detail::quote_argument(value);
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(command.c_str(), &argc);
    require(argv != nullptr, "CommandLineToArgvW failed");
    const bool matches = argc == 2 && std::wstring(argv[1]) == value;
    LocalFree(argv);
    require(matches, "Windows quote_argument round trip failed");
  }
}
#endif

int main(int argc, char** argv) {
  try {
    require(argc >= 2, "fixture mode required");
    const std::string mode = argv[1];
    // A real native child executable checks both its path and the received argv.
    if (mode == "--echo-args") {
      for (int i = 2; i < argc; ++i) {
        std::cout << hex(argv[i]) << '\n';
      }
      return 0;
    }
    if (mode == "literal") {
      emit(capture_shell_stdout("printf '%s' " + shell_quote(
        "spaces ' quote \" double $HOME `echo unsafe` & ; | %PATH% ! trailing \\")));
    } else if (mode == "double-quoted-path") {
      const fs::path path("spaces ' dollar $HOME `echo unsafe` & semicolon ; %PATH%");
      emit(capture_shell_stdout("printf '%s' \"" + shell_path_contents(path) + "\""));
    } else if (mode == "large-nonzero") {
      emit(capture_shell_stdout(
        "i=0; while [ \"$i\" -lt 10000 ]; do printf 0123456789; i=$((i+1)); done; exit 23"));
    } else if (mode == "pipeline") {
      emit(capture_shell_stdout(
        "printf 'alpha\\nbeta\\n' | (read first; read second; "
        "printf '%s:%s' \"$second\" \"$first\"; printf ignored >&2) 2>/dev/null"));
    } else if (mode == "stderr") {
      emit(capture_shell_stdout("(printf diagnostic >&2) 2>&1"));
    } else if (mode == "native-files") {
      const fs::path root = fs::absolute(argv[0]).parent_path();
      const fs::path input = root / "input ' & $HOME %PATH%.txt";
      const fs::path error = root / "stderr ' & $HOME %PATH%.txt";
      {
        std::ofstream out(input, std::ios::binary);
        require(out.is_open(), "cannot create native input fixture");
        out << "from native file\n";
      }
      const auto capture = capture_shell_stdout(
        "(read line; printf '%s' \"$line\"; printf diagnostic >&2) < "
        + shell_path(input) + " 2> " + shell_path(error));
      std::ifstream in(error, std::ios::binary);
      std::ostringstream content;
      content << in.rdbuf();
      require(content.str() == "diagnostic", "native stderr file differs");
      emit(capture);
    } else if (mode == "native-argv") {
      const std::vector<std::string> values = {
        "", "one two", "single'quote", "double\"quote", "trailing\\",
        "backslash\\\"quote", "$HOME `echo unsafe` & ; %PATH% !",
        "C:\\path with spaces\\file.txt"
      };
      std::string command = shell_path(fs::absolute(argv[0])) + " --echo-args";
      for (const auto& value : values) {
        command += " " + shell_quote(value);
      }
      emit(capture_shell_stdout(command));
    } else if (mode == "windows-quote") {
#ifdef _WIN32
      verify_windows_quoting();
      std::cout << "Windows argument fixtures passed\n";
#else
      throw std::runtime_error("Windows fixture requested on a non-Windows host");
#endif
    } else if (mode == "configured-shell") {
      emit(capture_shell_stdout("exit 0"));
    } else {
      throw std::runtime_error("unknown fixture mode");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
'''


@unittest.skipUnless(NATIVE_COMPILER,
                     "Set STYIO_TEST_NATIVE_COMPILER to compile/run native process integration")
class NativeProcessPipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if os.name == "nt":
            bash = os.environ.get("STYIO_TEST_BASH", "")
            if not bash or not Path(bash).is_absolute() or not Path(bash).is_file():
                raise AssertionError("Windows integration requires absolute STYIO_TEST_BASH")
        cls.temporary = tempfile.TemporaryDirectory(
            prefix="styio process ' $HOME `literal` & ; %PATH% ")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.directory = Path(cls.temporary.name)
        cls.source = cls.directory / "process fixture.cpp"
        cls.binary = cls.directory / ("native fixture.exe" if os.name == "nt" else "native fixture")
        cls.source.write_text(HARNESS, encoding="utf-8")
        assert NATIVE_COMPILER is not None
        driver = Path(NATIVE_COMPILER).name.lower()
        if driver in {"cl", "cl.exe", "clang-cl", "clang-cl.exe"}:
            command = [NATIVE_COMPILER, "/nologo", "/std:c++20", "/EHsc", "/utf-8",
                       f"/I{ROOT / 'src'}", str(cls.source), f"/Fe{cls.binary}",
                       f"/Fo{cls.directory / 'fixture.obj'}", "shell32.lib"]
        else:
            command = [NATIVE_COMPILER, "-std=c++20", "-Wall", "-Wextra",
                       "-I", str(ROOT / "src"), str(cls.source), "-o", str(cls.binary)]
            if os.name == "nt":
                command.append("-lshell32")
        compiled = subprocess.run(command, cwd=cls.directory, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, timeout=120,
                                  encoding="utf-8", errors="replace")
        if compiled.returncode != 0 or not cls.binary.is_file():
            raise AssertionError(f"Native process fixture compilation failed:\n{compiled.stdout}")

    def run_fixture(self, mode: str, *, env: dict[str, str] | None = None) -> subprocess.CompletedProcess:
        return subprocess.run([str(self.binary), mode], cwd=self.directory, env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)

    def capture(self, mode: str) -> tuple[int, int, str]:
        completed = self.run_fixture(mode)
        self.assertEqual(completed.returncode, 0,
                         completed.stderr.decode("utf-8", errors="replace"))
        lines = completed.stdout.decode("ascii").splitlines()
        self.assertEqual(len(lines), 3, completed.stdout)
        return int(lines[0]), int(lines[1]), bytes.fromhex(lines[2]).decode("utf-8")

    def test_preserves_quoted_shell_metacharacters(self) -> None:
        exit_code, _, output = self.capture("literal")
        self.assertEqual(exit_code, 0)
        self.assertEqual(output, LITERAL)

    def test_preserves_double_quoted_path_contents(self) -> None:
        exit_code, _, output = self.capture("double-quoted-path")
        self.assertEqual(exit_code, 0)
        self.assertEqual(output, PATH_LITERAL)

    def test_captures_large_output_and_nonzero_exit_status(self) -> None:
        exit_code, raw_status, output = self.capture("large-nonzero")
        self.assertEqual(exit_code, 23)
        self.assertEqual(output, "0123456789" * 10000)
        if os.name == "nt":
            self.assertEqual(raw_status, 23)
        else:
            self.assertTrue(os.WIFEXITED(raw_status))
            self.assertEqual(os.WEXITSTATUS(raw_status), 23)

    def test_supports_pipelines_stderr_and_explicit_windows_shell(self) -> None:
        self.assertEqual(self.capture("pipeline")[::2], (0, "beta:alpha"))
        self.assertEqual(self.capture("stderr")[::2], (0, "diagnostic"))
        if os.name == "nt":
            # An unavailable configured Bash must fail rather than using cmd/WSL.
            for value in (None, "bash.exe", str(self.directory / "missing bash.exe")):
                with self.subTest(bash=value):
                    environment = dict(os.environ)
                    environment.pop("STYIO_TEST_BASH", None)
                    if value is not None:
                        environment["STYIO_TEST_BASH"] = value
                    completed = self.run_fixture("configured-shell", env=environment)
                    self.assertNotEqual(completed.returncode, 0)
                    self.assertIn(b"STYIO_TEST_BASH", completed.stderr)

    def test_native_executable_and_redirection_paths_with_spaces_and_metacharacters(self) -> None:
        self.assertEqual(self.capture("native-files")[::2], (0, "from native file"))
        exit_code, _, output = self.capture("native-argv")
        self.assertEqual(exit_code, 0)
        self.assertEqual(output, "".join(value.encode("utf-8").hex() + "\n" for value in ARGV_FIXTURES))
        if os.name == "nt":
            completed = self.run_fixture("windows-quote")
            self.assertEqual(completed.returncode, 0,
                             completed.stderr.decode("utf-8", errors="replace"))
            self.assertEqual(completed.stdout.decode().strip(), "Windows argument fixtures passed")


if __name__ == "__main__":
    unittest.main()
