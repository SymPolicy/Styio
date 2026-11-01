#!/usr/bin/env python3
"""Refusal-shape tests for scripts/delivery-gate.sh origin_repo_name."""

import os
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GATE = ROOT / "scripts/delivery-gate.sh"


def run_case(url: str) -> subprocess.CompletedProcess:
    script = "\n".join(
        [
            "set -u",
            f'source <(sed -n "/^origin_repo_name()/,/^}}/p" "{GATE}")',
            "tmp=$(mktemp -d)",
            "cd \"$tmp\" && git init -q && git remote add origin \"$ORIGIN_URL\"",
            "origin_repo_name",
        ]
    )
    env = dict(os.environ)
    env["ORIGIN_URL"] = url
    return subprocess.run(
        ["bash", "-c", script],
        env=env,
        capture_output=True,
        text=True,
    )


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
