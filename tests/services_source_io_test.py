#!/usr/bin/env python3
"""Exercise source-read errors through the public syntax-check executable."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

COMPILER: Path


class SyntaxSourceIO(unittest.TestCase):
    def setUp(self) -> None:
        self.workspace = tempfile.TemporaryDirectory(prefix="styio-source-io-")
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name)

    def check(self, path: Path, code: int) -> dict:
        result = subprocess.run(
            [str(COMPILER), "check", "--syntax", "--json", "--file", str(path)],
            cwd=self.root, input="", capture_output=True, text=True, timeout=15,
        )
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        payload = json.loads(result.stdout)
        self.assertEqual(payload["contract"], "syntax-check")
        self.assertEqual(payload["file"], str(path))
        self.assertIs(payload["ok"], code == 0)
        self.assertEqual(payload["status"], "ok" if code == 0 else "cli_error")
        if code == 0:
            self.assertEqual(payload["diagnostics"], [])
        else:
            self.assertEqual(payload["phase"], "service")
            self.assertGreater(len(payload["diagnostics"]), 0)
        return payload

    def test_directory_is_not_an_empty_source(self) -> None:
        self.check(self.root, 6)

    def test_missing_source_is_a_cli_error(self) -> None:
        self.check(self.root / "missing.styio", 6)

    def test_empty_regular_source_is_valid(self) -> None:
        source = self.root / "empty.styio"
        source.write_bytes(b"")
        self.check(source, 0)

    def test_read_buffer_boundaries(self) -> None:
        for padding in (1, 8191, 8192, 8193, 16384, 16385):
            with self.subTest(padding=padding):
                source = self.root / "boundary.styio"
                source.write_text(" " * padding + "x := 1\n", encoding="utf-8")
                self.check(source, 0)

    def test_path_escaping_preserves_json(self) -> None:
        source = self.root / 'quote-"-name.styio'
        source.write_text("x := 1\n", encoding="utf-8")
        self.check(source, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    args, remaining = parser.parse_known_args()
    COMPILER = args.compiler.resolve(strict=True)
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
