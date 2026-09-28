#!/usr/bin/env python3
"""Black-box acceptance of the published syntax-check CLI; no source imports."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

COMPILER: Path


class SyntaxCheckContract(unittest.TestCase):
    def setUp(self) -> None:
        self.workspace = tempfile.TemporaryDirectory(prefix="styio-syntax-contract-")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)

    def source(self, text: str, name: str = "input.styio") -> Path:
        path = self.root / name
        path.write_text(text, encoding="utf-8")
        return path

    def check(self, path: Path, exit_code: int = 0, status: str = "ok", *options: str) -> dict:
        result = subprocess.run(
            [str(COMPILER), "check", "--syntax", "--json", "--file", str(path), *options],
            cwd=self.root, input="", capture_output=True, text=True, encoding="utf-8", timeout=15,
        )
        self.assertEqual(result.returncode, exit_code, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "", "Diagnostics must stay in the JSON response")
        payload = json.loads(result.stdout)
        self.assertIsInstance(payload, dict)
        self.assertEqual(payload["schema_version"], 1)
        self.assertEqual(payload["contract"], "syntax-check")
        self.assertEqual(payload["file"], str(path))
        self.assertEqual(payload["status"], status)
        self.assertIs(payload["ok"], exit_code == 0)
        self.assertIsInstance(payload["diagnostics"], list)
        if exit_code == 0:
            self.assertEqual(payload["diagnostics"], [])
        else:
            self.assertGreater(len(payload["diagnostics"]), 0)
        return payload

    def test_does_not_type_check(self) -> None:
        self.check(self.source("result: i32 := missing_symbol\n"))

    def test_empty_regular_file_is_not_an_io_error(self) -> None:
        self.check(self.source(""))

    def test_directory_is_a_cli_error(self) -> None:
        payload = self.check(self.root, 6, "cli_error")
        self.assertEqual(payload["phase"], "cli")

    def test_missing_file_is_a_cli_error(self) -> None:
        self.check(self.root / "absent.styio", 6, "cli_error")

    def test_parse_failure_is_structured(self) -> None:
        payload = self.check(self.source("# broken := (a: i32) => a +\n"), 3, "syntax_error")
        self.assertEqual(payload["phase"], "parse")
        self.assertEqual(payload["diagnostics"][0]["code"], "STYIO_PARSE")

    def test_lex_failure_location(self) -> None:
        payload = self.check(self.source("x := 1\n/* unterminated\n"), 2, "lexical_error")
        diagnostic = payload["diagnostics"][0]
        self.assertEqual((diagnostic["line"], diagnostic["column"], diagnostic["offset"]), (2, 1, 7))

    def test_does_not_write_runtime_file(self) -> None:
        target = self.root / "must-not-exist.txt"
        path = self.source('"sentinel" >> @file(' + json.dumps(str(target)) + ')\n')
        before = sorted(p.name for p in self.root.iterdir())
        self.check(path)
        self.assertFalse(target.exists())
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), before)

    def test_does_not_execute_stdout(self) -> None:
        self.check(self.source('"must-not-execute" >> @stdout\n'))

    def test_read_block_boundaries_preserve_parse_input(self) -> None:
        for padding in (8191, 8192, 8193, 16384):
            with self.subTest(padding=padding):
                self.check(self.source(" " * padding + "result: i32 := missing_symbol\n"))

    def test_path_is_json_escaped(self) -> None:
        self.check(self.source("x := 1\n", 'quoted-"-name.styio'))

    def test_unknown_option_is_a_cli_error(self) -> None:
        self.check(self.source("x := 1\n"), 6, "cli_error", "--not-a-real-option")

    def test_unknown_parser_engine_is_a_cli_error(self) -> None:
        self.check(self.source("x := 1\n"), 6, "cli_error", "--parser-engine=unknown")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    args, remaining = parser.parse_known_args()
    COMPILER = args.compiler.resolve(strict=True)
    if not COMPILER.is_file():
        parser.error("compiler must be an executable file")
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
