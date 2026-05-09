#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

ROOT = Path(__file__).resolve().parents[1]
ZERO_SHA = "0" * 40


@dataclass(frozen=True)
class Tool:
    key: str
    phase: int
    owner: str
    responsibility: str
    command: tuple[str, ...]


@dataclass(frozen=True)
class WorkflowDoc:
    key: str
    path: str
    phase: int
    responsibility: str


@dataclass(frozen=True)
class Profile:
    key: str
    purpose: str
    tools: tuple[str, ...]


TOOLS: tuple[Tool, ...] = (
    Tool(
        "workflow-scheduler-check",
        5,
        "Docs / Ecosystem",
        "Validate workflow registry, separation, ordering, and discoverability.",
        ("python3", "scripts/workflow-scheduler.py", "check"),
    ),
    Tool(
        "workflow-scheduler-tests",
        6,
        "Test Quality",
        "Run scheduler unit tests for range resolution and registry invariants.",
        ("python3", "tests/workflow_scheduler_test.py"),
    ),
    Tool(
        "repo-hygiene-worktree",
        10,
        "Docs / Ecosystem",
        "Reject forbidden worktree artifacts before local delivery.",
        ("python3", "scripts/repo-hygiene-gate.py", "--mode", "worktree"),
    ),
    Tool(
        "repo-hygiene-tracked",
        10,
        "Docs / Ecosystem",
        "Reject forbidden tracked artifacts and repository cleanliness drift.",
        ("python3", "scripts/repo-hygiene-gate.py", "--mode", "tracked"),
    ),
    Tool(
        "repo-hygiene-staged",
        10,
        "Docs / Ecosystem",
        "Reject forbidden staged artifacts before checkpoint delivery.",
        ("python3", "scripts/repo-hygiene-gate.py", "--mode", "staged"),
    ),
    Tool(
        "repo-hygiene-push",
        10,
        "Docs / Ecosystem",
        "Reject forbidden artifacts introduced by the incoming revision range.",
        ("python3", "scripts/repo-hygiene-gate.py", "--mode", "push", "--range", "{range}"),
    ),
    Tool(
        "runtime-surface",
        20,
        "Codegen / Runtime",
        "Keep codegen helper calls, ExternLib exports, implementations, and ORC registrations aligned.",
        ("python3", "scripts/runtime-surface-gate.py"),
    ),
    Tool(
        "team-docs-worktree",
        30,
        "Docs / Ecosystem",
        "Require runbook updates for owned worktree changes.",
        ("python3", "scripts/team-docs-gate.py"),
    ),
    Tool(
        "team-docs-staged",
        30,
        "Docs / Ecosystem",
        "Require runbook updates for staged checkpoint changes.",
        ("python3", "scripts/team-docs-gate.py", "--mode", "staged"),
    ),
    Tool(
        "team-docs-base",
        30,
        "Docs / Ecosystem",
        "Require runbook updates for branch changes against a base ref.",
        ("python3", "scripts/team-docs-gate.py", "--base", "{base}"),
    ),
    Tool(
        "docs-audit",
        35,
        "Docs / Ecosystem",
        "Validate docs metadata, links, lifecycle state, and generated-index freshness.",
        ("python3", "scripts/docs-audit.py"),
    ),
    Tool(
        "ecosystem-cli-docs",
        40,
        "Docs / Ecosystem",
        "Validate local ecosystem CLI contract documentation.",
        ("python3", "scripts/ecosystem-cli-doc-gate.py"),
    ),
    Tool(
        "ecosystem-cli-docs-workspace",
        40,
        "Docs / Ecosystem",
        "Validate ecosystem CLI contract docs against checked-out sibling repositories.",
        ("python3", "scripts/ecosystem-cli-doc-gate.py", "--require-workspace", "--json"),
    ),
)


WORKFLOW_DOCS: tuple[WorkflowDoc, ...] = (
    WorkflowDoc(
        "add-repo-file",
        "docs/assets/workflow/ADD-REPO-FILE.md",
        15,
        "Repository file creation with metadata, indexes, ownership, and gates.",
    ),
    WorkflowDoc(
        "add-resource-identifier",
        "docs/assets/workflow/ADD-RESOURCE-IDENTIFIER.md",
        25,
        "Resource identifier syntax, capability, lifecycle, and fail-closed rollout.",
    ),
    WorkflowDoc(
        "correct-syntax-contract",
        "docs/assets/workflow/CORRECT-SYNTAX-CONTRACT.md",
        25,
        "Syntax-contract correction from minimal repro through parser/Sema boundary, SSOT docs, and gates.",
    ),
    WorkflowDoc(
        "change-bootstrap-env",
        "docs/assets/workflow/CHANGE-BOOTSTRAP-ENV.md",
        20,
        "Bootstrap environment dependency, version, path, and documentation changes.",
    ),
    WorkflowDoc(
        "checkpoint-health",
        "docs/assets/workflow/CHECKPOINT-HEALTH.md",
        70,
        "Repository build/test health entrypoint for checkpoint delivery.",
    ),
    WorkflowDoc(
        "checkpoint",
        "docs/assets/workflow/CHECKPOINT-WORKFLOW.md",
        50,
        "Recovery-oriented checkpoint sizing and interruption handling.",
    ),
    WorkflowDoc(
        "delivery-gate",
        "docs/assets/workflow/DELIVERY-GATE.md",
        60,
        "Common delivery-floor entrypoint and health-gate handoff.",
    ),
    WorkflowDoc(
        "docs-maintenance",
        "docs/assets/workflow/DOCS-MAINTENANCE-WORKFLOW.md",
        30,
        "Documentation metadata, generated indexes, and archive lifecycle.",
    ),
    WorkflowDoc(
        "docs-gate",
        "docs/assets/workflow/DOCS-GATE.md",
        35,
        "Common docs/process gate entrypoint and composition boundary.",
    ),
    WorkflowDoc(
        "five-layer-pipeline",
        "docs/assets/workflow/FIVE-LAYER-PIPELINE.md",
        45,
        "Layered compiler golden coverage from lexer through runtime output.",
    ),
    WorkflowDoc(
        "repo-hygiene",
        "docs/assets/workflow/REPO-HYGIENE-COMMIT-STANDARD.md",
        10,
        "Repository cleanliness, commit, push, and history rewriting standards.",
    ),
    WorkflowDoc(
        "promote-nightly-parser-subset",
        "docs/assets/workflow/PROMOTE-NIGHTLY-PARSER-SUBSET.md",
        25,
        "Nightly parser subset promotion with parity, fallback, and error-boundary tests.",
    ),
    WorkflowDoc(
        "syntax-addition",
        "docs/assets/workflow/SYNTAX-ADDITION-WORKFLOW.md",
        25,
        "Ordered syntax-change chain from language SSOT through runtime registration.",
    ),
    WorkflowDoc(
        "team-runbook-maintenance",
        "docs/assets/workflow/TEAM-RUNBOOK-MAINTENANCE-GATE.md",
        30,
        "Team runbook ownership and update requirements for touched surfaces.",
    ),
    WorkflowDoc(
        "test-catalog",
        "docs/assets/workflow/TEST-CATALOG.md",
        45,
        "Named test inventory, fixtures, labels, and automation evidence.",
    ),
    WorkflowDoc(
        "workflow-orchestration",
        "docs/assets/workflow/WORKFLOW-ORCHESTRATION.md",
        5,
        "Tool registry, workflow separation, profile ordering, and scheduler usage.",
    ),
)


PROFILES: tuple[Profile, ...] = (
    Profile(
        "delivery-checkpoint",
        "Common worktree process gates for checkpoint delivery before optional checkpoint-health.",
        (
            "repo-hygiene-worktree",
            "runtime-surface",
            "team-docs-worktree",
            "docs-audit",
            "ecosystem-cli-docs",
        ),
    ),
    Profile(
        "delivery-staged",
        "Common staged-index process gates for commit hooks and staged handoff.",
        (
            "repo-hygiene-staged",
            "runtime-surface",
            "team-docs-staged",
            "docs-audit",
            "ecosystem-cli-docs",
        ),
    ),
    Profile(
        "delivery-push",
        "Common process gates for branch handoff against a base ref and incoming revision range.",
        (
            "repo-hygiene-push",
            "runtime-surface",
            "team-docs-base",
            "docs-audit",
            "ecosystem-cli-docs",
        ),
    ),
    Profile(
        "syntax-local",
        "Local syntax-change worktree verification.",
        (
            "workflow-scheduler-check",
            "workflow-scheduler-tests",
            "repo-hygiene-tracked",
            "runtime-surface",
            "team-docs-worktree",
            "docs-audit",
            "ecosystem-cli-docs",
        ),
    ),
    Profile(
        "ci-prebuild",
        "GitHub Actions prebuild chain before compile, CTest, parser-shadow, and fuzz smoke.",
        (
            "workflow-scheduler-check",
            "workflow-scheduler-tests",
            "repo-hygiene-tracked",
            "repo-hygiene-push",
            "runtime-surface",
            "team-docs-base",
            "ecosystem-cli-docs-workspace",
        ),
    ),
)


def by_key(items: Iterable[object]) -> dict[str, object]:
    output: dict[str, object] = {}
    for item in items:
        key = getattr(item, "key")
        if key in output:
            raise ValueError(f"duplicate key: {key}")
        output[key] = item
    return output


TOOLS_BY_KEY = by_key(TOOLS)
DOCS_BY_KEY = by_key(WORKFLOW_DOCS)
PROFILES_BY_KEY = by_key(PROFILES)


def run_git(args: Sequence[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True)


def default_upstream_base() -> str:
    proc = run_git(["rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"])
    if proc.returncode == 0:
        return proc.stdout.strip()
    return ""


def resolve_range(args: argparse.Namespace) -> str:
    if args.range:
        return args.range
    if args.event_name == "pull_request" and args.pull_request_base_sha and args.pull_request_head_sha:
        return f"{args.pull_request_base_sha}..{args.pull_request_head_sha}"
    if args.before_sha:
        if args.before_sha == ZERO_SHA:
            return "HEAD"
        if args.sha:
            return f"{args.before_sha}..{args.sha}"
    raise ValueError("profile requires --range or CI event SHA inputs")


def resolve_base(args: argparse.Namespace) -> str:
    if args.base:
        return args.base
    if args.event_name == "pull_request" and args.pull_request_base_sha:
        return args.pull_request_base_sha
    if args.before_sha and args.before_sha != ZERO_SHA:
        return args.before_sha
    upstream = default_upstream_base()
    if upstream:
        return upstream
    raise ValueError("profile requires --base or CI event base/before SHA")


def materialize_command(tool: Tool, args: argparse.Namespace) -> list[str]:
    values = {
        "base": resolve_base(args) if "{base}" in tool.command else "",
        "range": resolve_range(args) if "{range}" in tool.command else "",
    }
    return [part.format(**values) for part in tool.command]


def validate_registry() -> list[str]:
    errors: list[str] = []

    for collection, label in ((TOOLS, "tool"), (WORKFLOW_DOCS, "workflow doc"), (PROFILES, "profile")):
        seen: set[str] = set()
        for item in collection:
            if item.key in seen:
                errors.append(f"duplicate {label} key: {item.key}")
            seen.add(item.key)

    responsibilities: dict[str, str] = {}
    for tool in TOOLS:
        previous = responsibilities.setdefault(tool.responsibility, tool.key)
        if previous != tool.key:
            errors.append(f"tool responsibility overlaps: {previous} and {tool.key}")

    docs_responsibilities: dict[str, str] = {}
    for doc in WORKFLOW_DOCS:
        previous = docs_responsibilities.setdefault(doc.responsibility, doc.key)
        if previous != doc.key:
            errors.append(f"workflow responsibility overlaps: {previous} and {doc.key}")
        if not (ROOT / doc.path).exists():
            errors.append(f"registered workflow doc is missing: {doc.path}")

    active_docs = {
        path.relative_to(ROOT).as_posix()
        for path in (ROOT / "docs/assets/workflow").glob("*.md")
        if path.name not in {"README.md", "INDEX.md"}
    }
    registered_docs = {doc.path for doc in WORKFLOW_DOCS}
    for path in sorted(active_docs - registered_docs):
        errors.append(f"workflow doc is not registered in scripts/workflow-scheduler.py: {path}")
    for path in sorted(registered_docs - active_docs):
        errors.append(f"registered workflow doc does not exist in docs/assets/workflow: {path}")

    for profile in PROFILES:
        phases: list[int] = []
        for key in profile.tools:
            tool = TOOLS_BY_KEY.get(key)
            if tool is None:
                errors.append(f"profile {profile.key} references unknown tool: {key}")
                continue
            assert isinstance(tool, Tool)
            phases.append(tool.phase)
        if phases != sorted(phases):
            errors.append(f"profile {profile.key} tool phases are not ordered: {profile.tools}")

    orchestration_doc = ROOT / "docs/assets/workflow/WORKFLOW-ORCHESTRATION.md"
    if orchestration_doc.exists() and markdown_table() not in orchestration_doc.read_text(encoding="utf-8"):
        errors.append("WORKFLOW-ORCHESTRATION.md audit table is not synchronized with workflow-scheduler.py")

    return errors


def cmd_check(_: argparse.Namespace) -> int:
    errors = validate_registry()
    if errors:
        print("workflow scheduler check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print("workflow scheduler check passed")
    return 0


def markdown_table() -> str:
    lines = [
        "| Type | Key | Phase | Owner | Responsibility | Command / Path |",
        "|------|-----|-------|-------|----------------|----------------|",
    ]
    for doc in sorted(WORKFLOW_DOCS, key=lambda item: (item.phase, item.key)):
        lines.append(
            f"| Workflow | `{doc.key}` | {doc.phase} | docs | {doc.responsibility} | `{doc.path}` |"
        )
    for tool in sorted(TOOLS, key=lambda item: (item.phase, item.key)):
        command = " ".join(tool.command)
        lines.append(
            f"| Tool | `{tool.key}` | {tool.phase} | {tool.owner} | {tool.responsibility} | `{command}` |"
        )
    return "\n".join(lines)


def cmd_list(args: argparse.Namespace) -> int:
    if args.format == "markdown":
        print(markdown_table())
        return 0

    for profile in PROFILES:
        print(f"{profile.key}: {profile.purpose}")
        for key in profile.tools:
            tool = TOOLS_BY_KEY[key]
            assert isinstance(tool, Tool)
            print(f"  - {key}: {' '.join(tool.command)}")
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    profile = PROFILES_BY_KEY.get(args.profile)
    if profile is None:
        print(f"unknown profile: {args.profile}", file=sys.stderr)
        return 2
    assert isinstance(profile, Profile)

    skip = set(args.skip_tool or [])
    for key in profile.tools:
        if key in skip:
            print(f"[workflow-scheduler] skip {key}")
            continue
        tool = TOOLS_BY_KEY[key]
        assert isinstance(tool, Tool)
        try:
            command = materialize_command(tool, args)
        except ValueError as exc:
            print(f"[workflow-scheduler] {key}: {exc}", file=sys.stderr)
            return 2
        print(f"[workflow-scheduler] {key}: {' '.join(command)}", flush=True)
        proc = subprocess.run(command, cwd=ROOT)
        if proc.returncode != 0:
            return proc.returncode
    print(f"[workflow-scheduler] profile {profile.key} passed")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run and validate Styio workflow tool chains.")
    sub = parser.add_subparsers(dest="command", required=True)

    check = sub.add_parser("check", help="Validate workflow registry and separation invariants.")
    check.set_defaults(func=cmd_check)

    list_cmd = sub.add_parser("list", help="List registered profiles, tools, and workflow docs.")
    list_cmd.add_argument("--format", choices=["text", "markdown"], default="text")
    list_cmd.set_defaults(func=cmd_list)

    run = sub.add_parser("run", help="Run a registered workflow profile.")
    run.add_argument("--profile", required=True, choices=sorted(PROFILES_BY_KEY))
    run.add_argument("--base", default=os.environ.get("STYIO_WORKFLOW_BASE", ""))
    run.add_argument("--range", default=os.environ.get("STYIO_WORKFLOW_RANGE", ""))
    run.add_argument("--event-name", default=os.environ.get("GITHUB_EVENT_NAME", ""))
    run.add_argument("--before-sha", default=os.environ.get("GITHUB_EVENT_BEFORE", ""))
    run.add_argument("--sha", default=os.environ.get("GITHUB_SHA", ""))
    run.add_argument("--pull-request-base-sha", default=os.environ.get("GITHUB_PR_BASE_SHA", ""))
    run.add_argument("--pull-request-head-sha", default=os.environ.get("GITHUB_PR_HEAD_SHA", ""))
    run.add_argument("--skip-tool", action="append", choices=sorted(TOOLS_BY_KEY))
    run.set_defaults(func=cmd_run)

    return parser.parse_args()


def main() -> int:
    args = parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
