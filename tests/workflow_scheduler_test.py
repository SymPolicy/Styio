#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
SCHEDULER_PATH = ROOT / "scripts/workflow-scheduler.py"

spec = importlib.util.spec_from_file_location("workflow_scheduler", SCHEDULER_PATH)
assert spec is not None and spec.loader is not None
workflow_scheduler = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = workflow_scheduler
spec.loader.exec_module(workflow_scheduler)


class WorkflowSchedulerTest(unittest.TestCase):
    def test_registry_invariants_hold(self) -> None:
        self.assertEqual([], workflow_scheduler.validate_registry())

    def test_ci_pull_request_range_resolution(self) -> None:
        args = SimpleNamespace(
            range="",
            event_name="pull_request",
            pull_request_base_sha="base123",
            pull_request_head_sha="head456",
            before_sha="",
            sha="",
        )
        self.assertEqual("base123..head456", workflow_scheduler.resolve_range(args))

    def test_push_creation_range_resolution(self) -> None:
        args = SimpleNamespace(
            range="",
            event_name="push",
            pull_request_base_sha="",
            pull_request_head_sha="",
            before_sha=workflow_scheduler.ZERO_SHA,
            sha="head456",
        )
        self.assertEqual("HEAD", workflow_scheduler.resolve_range(args))

    def test_profile_phases_are_ordered(self) -> None:
        for profile in workflow_scheduler.PROFILES:
            phases = [workflow_scheduler.TOOLS_BY_KEY[key].phase for key in profile.tools]
            self.assertEqual(sorted(phases), phases, profile.key)

    def test_ecosystem_workspace_gate_triggers_on_contract_files(self) -> None:
        self.assertTrue(
            workflow_scheduler.ecosystem_workspace_gate_required(
                ["docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md"]
            )
        )
        self.assertTrue(
            workflow_scheduler.ecosystem_workspace_gate_required(
                ["scripts/ecosystem-cli-doc-gate.py"]
            )
        )

    def test_ecosystem_workspace_gate_skips_unrelated_lsp_changes(self) -> None:
        self.assertFalse(
            workflow_scheduler.ecosystem_workspace_gate_required(
                [
                    "src/StyioServices/StyioLSP/main.cpp",
                    "tests/lsp_stdio_framing_test.py",
                    ".github/workflows/styio-ci-gate.yml",
                ]
            )
        )

    def test_markdown_table_exposes_syntax_workflow(self) -> None:
        table = workflow_scheduler.markdown_table()
        self.assertIn("FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md", table)
        self.assertIn("FEATURE-CUTOVER-WORKFLOW.md", table)
        self.assertIn("LOCAL-INFO-LEAK-GATE.md", table)
        self.assertIn("ADD-SYNTAX-WITH-SKILLS.md", table)
        self.assertIn("local-info-leak-worktree", table)
        self.assertIn("runtime-surface", table)


if __name__ == "__main__":
    unittest.main()
