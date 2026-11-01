#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCS_AUDIT_PATH = ROOT / "scripts/docs-audit.py"

spec = importlib.util.spec_from_file_location("docs_audit", DOCS_AUDIT_PATH)
assert spec is not None and spec.loader is not None
docs_audit = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = docs_audit
sys.path.insert(0, str(ROOT / "scripts"))
spec.loader.exec_module(docs_audit)

HEADER = "| Team | Document | Word count | Character count |\n|------|----------|------------|-----------------|\n"
ROW = "| Test / Team | [TEST-RUNBOOK.md](./TEST-RUNBOOK.md) | 1 | 1 |\n"
TOTAL = "| **Total** | Team runbooks only | **1** | **1** |\n"
RUNBOOK = "# Test / Team Runbook\n\n**Purpose:** fixture\n\nbody words\n"


class RenderTeamStatsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        (root / "docs/teams").mkdir(parents=True)
        self._old_root = docs_audit.ROOT
        docs_audit.ROOT = root
        self.stats = root / "docs/teams/DOC-STATS.md"

    def tearDown(self) -> None:
        docs_audit.ROOT = self._old_root
        self.tmp.cleanup()

    def write(self, table: str, runbooks: dict[str, str] | None = None) -> None:
        (self.stats.parent / "TEST-RUNBOOK.md").write_text(runbooks or RUNBOOK, encoding="utf-8")
        self.stats.write_text("# Stats\n\n" + table, encoding="utf-8")

    def render(self) -> str:
        return docs_audit.render_team_stats(Path("docs/teams/DOC-STATS.md"))

    def test_happy_path_rewrites_numbers(self) -> None:
        self.write(HEADER + ROW + TOTAL)
        out = self.render()
        self.assertIn("TEST-RUNBOOK.md", out)
        self.assertIn("**Total**", out)
        self.assertNotIn("| 1 | 1 |", out)

    def test_missing_total_refuses(self) -> None:
        self.write(HEADER + ROW)
        with self.assertRaises(ValueError):
            self.render()

    def test_duplicate_total_refuses(self) -> None:
        self.write(HEADER + ROW + TOTAL + TOTAL)
        with self.assertRaises(ValueError):
            self.render()

    def test_no_runbook_rows_refuses(self) -> None:
        self.write(HEADER + TOTAL)
        with self.assertRaises(ValueError):
            self.render()

    def test_escaping_link_refuses(self) -> None:
        self.write(HEADER + "| T | [TEST-RUNBOOK.md](../TEST-RUNBOOK.md) | 1 | 1 |\n" + TOTAL)
        with self.assertRaises(ValueError):
            self.render()

    def test_unresolvable_link_refuses(self) -> None:
        self.write(HEADER + "| T | [GHOST-RUNBOOK.md](./GHOST-RUNBOOK.md) | 1 | 1 |\n" + TOTAL)
        with self.assertRaises(ValueError):
            self.render()

    def test_disk_runbook_without_row_refuses(self) -> None:
        self.write(HEADER + ROW + TOTAL)
        (self.stats.parent / "EXTRA-RUNBOOK.md").write_text(RUNBOOK, encoding="utf-8")
        with self.assertRaises(ValueError):
            self.render()

    def test_label_target_mismatch_refuses(self) -> None:
        self.write(HEADER + "| T | [OTHER-RUNBOOK.md](./TEST-RUNBOOK.md) | 1 | 1 |\n" + TOTAL)
        with self.assertRaises(ValueError):
            self.render()

    def test_duplicate_target_refuses(self) -> None:
        self.write(HEADER + ROW + "| T2 | [TEST-RUNBOOK.md](./sub/../TEST-RUNBOOK.md) | 1 | 1 |\n" + TOTAL)
        with self.assertRaises(ValueError):
            self.render()

    def test_stats_outside_root_refuses(self) -> None:
        self.write(HEADER + ROW + TOTAL)
        with self.assertRaises(ValueError):
            docs_audit.render_team_stats(Path("/etc/hosts"))


if __name__ == "__main__":
    unittest.main()
