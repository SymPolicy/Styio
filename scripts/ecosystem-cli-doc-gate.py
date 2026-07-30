#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORKSPACE_ROOT = ROOT.parent
REQUIRED_REPOS = ("styio-nightly", "pafio-nightly", "vityo-nightly")


@dataclass(frozen=True)
class DocRule:
    path: str
    needles: tuple[str, ...]
    workspace_only: bool = False


RULES: tuple[DocRule, ...] = (
    DocRule(
        "styio-nightly/docs/plan/Styio-Ecosystem-CLI-Contract-Matrix.md",
        (
            '### 2.1 `styio --machine-info=json`',
            '### 2.2 `styio --compile-plan <path>`',
            '`generated_by.tool = "pafio"`',
            "### 3.1 `pafio metadata --json`",
            "### 3.2 `pafio --json check/build/run/test`",
            "`--styio-bin`, `PAFIO_STYIO_BIN`, then",
            "Styio Platform owns the registry service",
            "Vityo does not inspect `PAFIO_HOME`",
        ),
    ),
    DocRule(
        "styio-nightly/docs/external/for-pafio/Styio-Nano-Pafio-Coordination.md",
        (
            "system-provided",
            "`styio --machine-info=json`",
            "`styio --compile-plan <path>`",
            '"tool": "pafio"',
            "Pafio does not install, update, switch, pin, build, or cache Styio",
        ),
    ),
    DocRule(
        "pafio-nightly/docs/governance/Pafio-CLI-Contract.md",
        (
            "pafio metadata --json",
            "metadata v1",
            "pafio --json check|build|run|test",
            "`--styio-bin`, `PAFIO_STYIO_BIN`, then `styio`",
            "never installs, updates,",
            "caches Styio",
        ),
        workspace_only=True,
    ),
    DocRule(
        "pafio-nightly/docs/external/for-styio/Styio-External-Interface-Requirement-Spec.md",
        (
            "`styio --machine-info=json`",
            "`styio --compile-plan <path>`",
            "compile-plan v1",
            "machine-readable diagnostics",
        ),
        workspace_only=True,
    ),
    DocRule(
        "vityo-nightly/docs/external/for-pafio/Pafio-Metadata-Contract.md",
        (
            "pafio metadata --json",
            "metadata v1",
            "`package`",
            "`workspace`",
            "`dependencies`",
            "`targets`",
            "`lock`",
            "`resolution`",
            "`vendor`",
            "`styio --machine-info=json`",
        ),
        workspace_only=True,
    ),
    DocRule(
        "vityo-nightly/docs/external/for-pafio/Pafio-Workflow-Success-Payloads.md",
        (
            "pafio --json build",
            "pafio --json run",
            "pafio --json test",
            "`receipt_path`",
            "`diagnostics_path`",
            "`runtime_events_path`",
        ),
        workspace_only=True,
    ),
    DocRule(
        "vityo-nightly/docs/external/for-styio/Styio-Compile-Run-Contract.md",
        (
            "`styio --compile-plan <path>`",
            "`pafio`",
            "`styio --machine-info=json`",
            "receipt",
            "diagnostics",
        ),
        workspace_only=True,
    ),
    DocRule(
        "vityo-nightly/docs/external/for-platform/Platform-Hosted-Workspace-Contract.md",
        (
            "Platform hosted-workspace v1",
            "`pafio build`",
            "system-provided Styio compiler",
        ),
        workspace_only=True,
    ),
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate the current Styio ecosystem machine-contract documentation."
    )
    parser.add_argument(
        "--workspace-root",
        type=Path,
        default=DEFAULT_WORKSPACE_ROOT,
        help="directory containing styio-nightly, pafio-nightly, and vityo-nightly",
    )
    parser.add_argument(
        "--require-workspace",
        action="store_true",
        help="require all sibling repositories and validate consumer mirrors",
    )
    parser.add_argument("--json", action="store_true", help="emit JSON")
    return parser


def check_rule(workspace_root: Path, rule: DocRule) -> dict:
    path = workspace_root / rule.path
    exists = path.is_file()
    missing: list[str] = []
    if exists:
        text = path.read_text(encoding="utf-8")
        missing = [needle for needle in rule.needles if needle not in text]
    return {
        "path": rule.path,
        "exists": exists,
        "missing_needles": missing,
        "ok": exists and not missing,
    }


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    workspace_root = args.workspace_root.resolve()
    local_workspace_root = ROOT.parent
    missing_repos = [
        repo for repo in REQUIRED_REPOS if not (workspace_root / repo).is_dir()
    ]

    if args.require_workspace and missing_repos:
        payload = {
            "contract": "styio-ecosystem-docs",
            "version": 1,
            "ok": False,
            "missing_repositories": missing_repos,
            "checks": [],
        }
    else:
        include_workspace = not missing_repos
        root = workspace_root if include_workspace else local_workspace_root
        checks = [
            check_rule(root, rule)
            for rule in RULES
            if include_workspace or not rule.workspace_only
        ]
        payload = {
            "contract": "styio-ecosystem-docs",
            "version": 1,
            "ok": all(check["ok"] for check in checks),
            "workspace_checked": include_workspace,
            "missing_repositories": missing_repos,
            "checks": checks,
        }

    if args.json:
        print(json.dumps(payload, sort_keys=True))
    else:
        for check in payload["checks"]:
            status = "OK" if check["ok"] else "FAIL"
            print(f"[{status}] {check['path']}")
            if not check["exists"]:
                print("  missing file")
            for needle in check["missing_needles"]:
                print(f"  missing: {needle}")
        if payload.get("missing_repositories") and not args.require_workspace:
            print(
                "[SKIP] consumer mirrors: missing "
                + ", ".join(payload["missing_repositories"])
            )
        print(
            "ecosystem CLI doc gate "
            + ("passed" if payload["ok"] else "failed")
        )

    return 0 if payload["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
