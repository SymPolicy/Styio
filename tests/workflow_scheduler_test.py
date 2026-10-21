#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import subprocess
import tempfile
import shutil
import os

ROOT = Path(__file__).resolve().parents[1]
SCHEDULER_PATH = ROOT / "scripts/workflow-scheduler.py"

spec = importlib.util.spec_from_file_location("workflow_scheduler", SCHEDULER_PATH)
assert spec is not None and spec.loader is not None
workflow_scheduler = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = workflow_scheduler
spec.loader.exec_module(workflow_scheduler)


class WorkflowSchedulerTest(unittest.TestCase):
    def delivery_args(self, profiles, skip=()):
        return SimpleNamespace(profile=profiles, skip_tool=skip, base="base", range="base..HEAD")

    def test_composed_delivery_runs_shared_checks_once(self):
        with patch.object(workflow_scheduler.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as run:
            self.assertEqual(0, workflow_scheduler.cmd_run(self.delivery_args(
                ["delivery-checkpoint", "delivery-push"])))
        commands = [call.args[0] for call in run.call_args_list]
        self.assertEqual(12, len(commands))  # Previously 18, plus two nested team checks.
        self.assertEqual(1, sum("scripts/docs-audit.py" in c for c in commands))
        self.assertIn(["python3", "scripts/team-docs-gate.py"], commands)
        self.assertIn(["python3", "scripts/team-docs-gate.py", "--base", "base"], commands)
        for call in run.call_args_list:
            if "scripts/docs-audit.py" in call.args[0]:
                self.assertEqual("1", call.kwargs["env"].get("STYIO_SKIP_TEAM_DOC_GATE"))

    def test_skipped_team_check_cannot_supply_evidence(self):
        with patch.object(workflow_scheduler.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as run:
            with patch.dict(os.environ, {"STYIO_SKIP_TEAM_DOC_GATE": "1"}):
                self.assertEqual(0, workflow_scheduler.cmd_run(self.delivery_args(
                    ["delivery-staged"], ["team-docs-staged"])))
        audit = next(c for c in run.call_args_list if "scripts/docs-audit.py" in c.args[0])
        self.assertNotIn("STYIO_SKIP_TEAM_DOC_GATE", audit.kwargs["env"])

    def test_failed_check_stops_composition_without_reuse(self):
        with patch.object(workflow_scheduler.subprocess, "run", return_value=SimpleNamespace(returncode=7)) as run:
            self.assertEqual(7, workflow_scheduler.cmd_run(self.delivery_args(
                ["delivery-checkpoint", "delivery-push"])))
            self.assertEqual(1, run.call_count)

    def test_new_invocation_runs_again(self):
        with patch.object(workflow_scheduler.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as run:
            args = self.delivery_args(["delivery-checkpoint"])
            workflow_scheduler.cmd_run(args)
            workflow_scheduler.cmd_run(args)
            self.assertEqual(18, run.call_count)

    def test_audit_with_and_without_scoped_evidence_are_distinct(self):
        with patch.object(workflow_scheduler.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as run:
            workflow_scheduler.cmd_run(self.delivery_args(
                ["delivery-checkpoint", "delivery-push"], ["team-docs-worktree"]))
        audits = [c for c in run.call_args_list if "scripts/docs-audit.py" in c.args[0]]
        self.assertEqual(2, len(audits))

    def test_delivery_auto_composes_profiles(self):
        # Execute the real shell entrypoint in an isolated fixture; no compiler,
        # audit service, or network is needed for this process-only behavior.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            # Fixture commits must not use the developer's signing or hooks.
            env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
            env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
            (root / "scripts").mkdir()
            shutil.copy(ROOT / "scripts/delivery-gate.sh", root / "scripts/delivery-gate.sh")
            (root / "scripts/workflow-scheduler.py").write_text(
                "import sys\nfrom pathlib import Path\n"
                "p=Path('calls');p.write_text(p.read_text()+repr(sys.argv[1:])+'\\n' if p.exists() else repr(sys.argv[1:])+'\\n')\n")
            for command in (["git", "init", "-q"], ["git", "-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--allow-empty", "-qm", "base"]):
                subprocess.run(command, cwd=root, env=env, check=True, capture_output=True)
            base = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, env=env, text=True).strip()
            subprocess.run(["git", "-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--allow-empty", "-qm", "task"], cwd=root, env=env, check=True)
            proc = subprocess.run(["bash", "scripts/delivery-gate.sh", "--base", base, "--skip-health"], cwd=root, env=env, capture_output=True, text=True)
            self.assertEqual(0, proc.returncode, proc.stderr)
            calls = (root / "calls").read_text().splitlines()
            self.assertEqual(1, len(calls))
            self.assertIn("delivery-checkpoint", calls[0])
            self.assertIn("delivery-push", calls[0])

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
                ["docs/external/for-pafio/Styio-Ecosystem-Machine-Contract-Matrix.md"]
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
        self.assertIn("syntax-feature-state", table)
        self.assertIn("runtime-surface", table)


    def _prepare_audit_gate_fixture(self, root: Path):
        # Isolated real-entry fixture: the real shell gate, a recording fake
        # scheduler and a recording fake audit action; no compiler, no audit
        # service and no network are needed.
        env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
        env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
        env.pop("CI", None)
        env.pop("GITHUB_ACTIONS", None)
        (root / "scripts").mkdir()
        shutil.copy(ROOT / "scripts/delivery-gate.sh", root / "scripts/delivery-gate.sh")
        (root / "scripts/workflow-scheduler.py").write_text(
            "import sys\nfrom pathlib import Path\n"
            "p=Path('calls');p.write_text(p.read_text()+repr(sys.argv[1:])+'\\n' if p.exists() else repr(sys.argv[1:])+'\\n')\n")
        audit_root = root / "audit"
        audit_root.mkdir()
        (audit_root / "action_entry.py").write_text(
            "import os, sys\nfrom pathlib import Path\n"
            "record = Path(os.environ['FAKE_ACTION_RECORD'])\n"
            "with record.open('a') as fh:\n"
            "    fh.write(' '.join(sys.argv[1:]) + '\\n')\n"
            "scope = sys.argv[sys.argv.index('--scope') + 1]\n"
            "sys.exit(3 if os.environ.get('FAKE_ACTION_FAIL_SCOPE', '') == scope else 0)\n")
        for command in (["git", "init", "-q"], ["git", "-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--allow-empty", "-qm", "base"]):
            subprocess.run(command, cwd=root, env=env, check=True, capture_output=True)
        base = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, env=env, text=True).strip()
        subprocess.run(["git", "-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--allow-empty", "-qm", "task"], cwd=root, env=env, check=True, capture_output=True)
        return env, base, audit_root

    def _run_audit_gate(self, root: Path, env: dict, base: str, *extra):
        record = root / "audit-calls"
        if record.exists():
            record.unlink()
        proc = subprocess.run(
            ["bash", "scripts/delivery-gate.sh", "--base", base, "--skip-health", *extra],
            cwd=root, env=env, capture_output=True, text=True)
        calls = record.read_text().splitlines() if record.exists() else []
        return proc, calls

    def test_audit_gate_runs_both_scopes_from_one_identity_source(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            env, base, audit_root = self._prepare_audit_gate_fixture(root)
            env["GENERAL_AUDITOR_ROOT"] = str(audit_root)
            env["FAKE_ACTION_RECORD"] = str(root / "audit-calls")

            # Without an origin remote the gate refuses instead of guessing an
            # audit target.
            proc, calls = self._run_audit_gate(root, env, base)
            self.assertEqual(2, proc.returncode, proc.stderr)
            self.assertIn("repository identity", proc.stderr)
            self.assertEqual([], calls)

            subprocess.run(["git", "remote", "add", "origin", "git@github.com:Fixture/Origin-Repo.git"], cwd=root, env=env, check=True, capture_output=True)
            proc, calls = self._run_audit_gate(root, env, base)
            self.assertEqual(0, proc.returncode, proc.stderr)
            self.assertEqual(2, len(calls), calls)
            for line, scope in zip(calls, ("history", "worktree")):
                self.assertTrue(line.startswith("scan "), line)
                self.assertIn("--repository Fixture/Origin-Repo", line)
                self.assertIn(f"--scope {scope}", line)

            # Swapping the single configuration source changes both calls
            # together; neither scope keeps an independent copy.
            subprocess.run(["git", "remote", "set-url", "origin", "https://github.com/Fixture/Swapped-Repo.git"], cwd=root, env=env, check=True, capture_output=True)
            proc, calls = self._run_audit_gate(root, env, base)
            self.assertEqual(0, proc.returncode, proc.stderr)
            self.assertEqual(2, len(calls), calls)
            for line in calls:
                self.assertIn("--repository Fixture/Swapped-Repo", line)
                self.assertNotIn("Origin-Repo", line)

    def test_audit_gate_switches_to_check_under_ci_and_propagates_failures(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            env, base, audit_root = self._prepare_audit_gate_fixture(root)
            env["GENERAL_AUDITOR_ROOT"] = str(audit_root)
            env["FAKE_ACTION_RECORD"] = str(root / "audit-calls")
            subprocess.run(["git", "remote", "add", "origin", "git@github.com:Fixture/Origin-Repo.git"], cwd=root, env=env, check=True, capture_output=True)

            # CI flips the audit command for both scopes.
            ci_env = dict(env)
            ci_env["CI"] = "true"
            proc, calls = self._run_audit_gate(root, ci_env, base)
            self.assertEqual(0, proc.returncode, proc.stderr)
            self.assertEqual(2, len(calls), calls)
            for line in calls:
                self.assertTrue(line.startswith("check "), line)

            # A failing scope is reported and the other scope still runs; the
            # gate returns the failing status.
            fail_env = dict(env)
            fail_env["FAKE_ACTION_FAIL_SCOPE"] = "history"
            proc, calls = self._run_audit_gate(root, fail_env, base)
            self.assertEqual(3, proc.returncode, proc.stderr)
            self.assertEqual(2, len(calls), calls)
            self.assertIn("--scope history", calls[0])
            self.assertIn("--scope worktree", calls[1])

            fail_env["FAKE_ACTION_FAIL_SCOPE"] = "worktree"
            proc, calls = self._run_audit_gate(root, fail_env, base)
            self.assertEqual(3, proc.returncode, proc.stderr)
            self.assertEqual(2, len(calls), calls)

    def test_audit_gate_rejects_untrusted_roots(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            env, base, audit_root = self._prepare_audit_gate_fixture(root)
            env["FAKE_ACTION_RECORD"] = str(root / "audit-calls")
            subprocess.run(["git", "remote", "add", "origin", "git@github.com:Fixture/Origin-Repo.git"], cwd=root, env=env, check=True, capture_output=True)

            # Relative audit roots are refused.
            proc, calls = self._run_audit_gate(root, env, base, "--audit-root", "audit")
            self.assertEqual(2, proc.returncode, proc.stderr)
            self.assertIn("absolute trusted root", proc.stderr)
            self.assertEqual([], calls)

            # An absolute root without action_entry.py is refused.
            empty_root = root / "empty-audit"
            empty_root.mkdir()
            proc, calls = self._run_audit_gate(root, env, base, "--audit-root", str(empty_root))
            self.assertEqual(2, proc.returncode, proc.stderr)
            self.assertIn("action_entry.py", proc.stderr)
            self.assertEqual([], calls)

            # Without any configured root the gate refuses rather than
            # silently skipping the audit.
            no_root_env = dict(env)
            no_root_env.pop("GENERAL_AUDITOR_ROOT", None)
            proc, calls = self._run_audit_gate(root, no_root_env, base)
            self.assertEqual(2, proc.returncode, proc.stderr)
            self.assertIn("absolute trusted root", proc.stderr)
            self.assertEqual([], calls)


if __name__ == "__main__":
    unittest.main()
