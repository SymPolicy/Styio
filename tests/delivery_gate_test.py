#!/usr/bin/env python3
"""Refusal-shape tests for scripts/delivery-gate.sh origin_repo_name."""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GATE = ROOT / "scripts/delivery-gate.sh"

KEEP_ENV = ("PATH", "HOME", "TMPDIR", "LANG", "LC_ALL")


def run_case(url: str) -> subprocess.CompletedProcess:
    fixture = tempfile.mkdtemp(prefix="delivery-gate-test-")
    try:
        env = {k: v for k, v in os.environ.items() if k in KEEP_ENV}
        env["ORIGIN_URL"] = url
        env["FIXTURE_DIR"] = fixture
        script = "\n".join(
            [
                "set -u",
                f'source "{GATE}"',
                'cd "$FIXTURE_DIR" && git init -q && git remote add origin "$ORIGIN_URL"',
                "origin_repo_name",
            ]
        )
        return subprocess.run(
            ["bash", "-c", script],
            env=env,
            capture_output=True,
            text=True,
        )
    finally:
        shutil.rmtree(fixture, ignore_errors=True)


class DeliveryGateOriginTest(unittest.TestCase):
    def test_cleartext_http_origin_refuses(self) -> None:
        proc = run_case("http://github.com/Acme/Widget.git")
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(proc.stdout.strip(), "")
        self.assertIn("cleartext http", proc.stderr)

    def test_https_origin_resolves_name(self) -> None:
        proc = run_case("https://github.com/Acme/Widget.git")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(proc.stdout.strip(), "Acme/Widget")

    def test_ssh_origin_resolves_name(self) -> None:
        proc = run_case("git@github.com:Acme/Widget.git")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(proc.stdout.strip(), "Acme/Widget")


if __name__ == "__main__":
    unittest.main()
