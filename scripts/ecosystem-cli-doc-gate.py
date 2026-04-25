#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORKSPACE_ROOT = ROOT.parent
REQUIRED_REPOS = ("styio-nightly", "styio-spio", "styio-view")


@dataclass(frozen=True)
class DocRule:
    path: str
    needles: tuple[str, ...]


@dataclass(frozen=True)
class ContractRule:
    key: str
    summary: str
    docs: tuple[DocRule, ...]


CONTRACT_RULES: tuple[ContractRule, ...] = (
    ContractRule(
        key="styio.machine_info",
        summary="styio machine-info handshake fields and compile-plan advertisement stay aligned",
        docs=(
            DocRule(
                "styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md",
                (
                    "### 2.1 `styio --machine-info=json`",
                    "`active_integration_phase`",
                    "`supported_contract_versions`",
                    "`supported_adapter_modes`",
                    "`feature_flags`",
                    "`supported_contracts.compile_plan:[1]`",
                ),
            ),
            DocRule(
                "styio-spio/docs/styio/Styio-External-Interface-Requirement-Spec.md",
                (
                    "### 2.1 `styio --machine-info=json`",
                    "`active_integration_phase`",
                    "`supported_contract_versions`",
                    "`supported_adapter_modes`",
                    "`feature_flags`",
                ),
            ),
            DocRule(
                "styio-view/docs/for-styio/Styio-Compile-Run-Contract.md",
                (
                    "`styio --machine-info=json`",
                    "`supported_contract_versions`",
                    "`supported_adapter_modes`",
                    "`feature_flags`",
                    "`supported_contracts.compile_plan:[1]`",
                ),
            ),
        ),
    ),
    ContractRule(
        key="styio.compile_plan",
        summary="styio compile-plan consumer behavior stays aligned for spio and view",
        docs=(
            DocRule(
                "styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md",
                (
                    "### 2.2 `styio --compile-plan <path>`",
                    "`build/check/run/test` 都走 compile-plan v1",
                    "`diagnostics.jsonl`",
                    "`CliError`",
                ),
            ),
            DocRule(
                "styio-spio/docs/styio/Styio-External-Interface-Requirement-Spec.md",
                (
                    "### 2.2 `styio --compile-plan <path>`",
                    "- `check`",
                    "invalid plan and CLI-conflict failures should remain machine-readable",
                    "diagnostics output declared in the plan",
                ),
            ),
            DocRule(
                "styio-view/docs/for-styio/Styio-Compile-Run-Contract.md",
                (
                    "`styio --compile-plan <path>`",
                    "`build/check/run/test` 都走同一条 compile-plan v1 入口",
                    "`diagnostics.jsonl`",
                    "`CliError`",
                ),
            ),
        ),
    ),
    ContractRule(
        key="spio.machine_info",
        summary="spio machine-info advertised contract lines stay aligned",
        docs=(
            DocRule(
                "styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md",
                (
                    "### 3.1 `spio machine-info --json`",
                    "`supported_contracts.project_graph:[1]`",
                    "`supported_contracts.toolchain_state:[1]`",
                    "`supported_contracts.workflow_success_payloads:[1]`",
                ),
            ),
            DocRule(
                "styio-spio/docs/governance/Spio-CLI-Contract.md",
                (
                    "spio machine-info --json",
                    "`supported_contracts.project_graph` reports `[1]`",
                    "`supported_contracts.toolchain_state` reports `[1]`",
                    "`supported_contracts.workflow_success_payloads` reports `[1]`",
                ),
            ),
            DocRule(
                "styio-view/docs/for-spio/Spio-Toolchain-And-Registry-State.md",
                (
                    "`spio machine-info --json`",
                    "`spio project-graph --json`",
                    "`spio tool status --json`",
                ),
            ),
        ),
    ),
    ContractRule(
        key="spio.project_graph",
        summary="spio project graph payload keys stay aligned",
        docs=(
            DocRule(
                "styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md",
                (
                    "### 3.2 `spio project-graph --manifest-path <path> --json`",
                    "`project_graph v1`",
                    "`package_distribution`",
                    "`source_state`",
                    "`managed_toolchains`",
                ),
            ),
            DocRule(
                "styio-spio/docs/governance/Spio-CLI-Contract.md",
                (
                    "`spio project-graph --json` publishes `project_graph v1`",
                    "`project_graph v1` includes at least `packages`, `dependencies`, `targets`, `toolchain`, `managed_toolchains`, `lock_state`, `vendor_state`, `notes`, `package_distribution`, and `source_state`",
                ),
            ),
            DocRule(
                "styio-view/docs/for-spio/Spio-Project-Graph-Contract.md",
                (
                    "`spio project-graph --manifest-path <path> --json`",
                    "`project_graph v1`",
                    "`managed_toolchains`",
                    "`package_distribution`",
                    "`source_state`",
                ),
            ),
        ),
    ),
    ContractRule(
        key="spio.toolchain_state",
        summary="spio toolchain state payload stays aligned",
        docs=(
            DocRule(
                "styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md",
                (
                    "### 3.3 `spio tool status --manifest-path <path> --json`",
                    "`toolchain_state v1`",
                    "`project_pin`",
                    "`current_compiler`",
                    "`managed_toolchains`",
                ),
            ),
            DocRule(
                "styio-spio/docs/governance/Spio-CLI-Contract.md",
                (
                    "`spio tool status --json` publishes `toolchain_state v1`",
                    "`project_pin`",
                    "`current_compiler`",
                    "`managed_toolchains`",
                ),
            ),
            DocRule(
                "styio-view/docs/for-spio/Spio-Toolchain-And-Registry-State.md",
                (
                    "`spio tool status --json`",
                    "`toolchain_state v1`",
                    "`project_pin`",
                    "`current_compiler`",
                    "`managed_toolchains`",
                ),
            ),
        ),
    ),
    ContractRule(
        key="spio.workflow_success",
        summary="spio build/run/test success payloads stay aligned",
        docs=(
            DocRule(
                "styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md",
                (
                    "### 3.4 `spio --json build/run/test`",
                    "`workflow_success_payloads v1`",
                    "`receipt_path`",
                    "`diagnostics_path`",
                    "`stdout / stderr`",
                ),
            ),
            DocRule(
                "styio-spio/docs/governance/Spio-CLI-Contract.md",
                (
                    "`workflow_success_payloads v1`",
                    "`receipt.json`",
                    "`diagnostics.jsonl` path",
                    "captured stdout/stderr",
                ),
            ),
            DocRule(
                "styio-view/docs/for-spio/Spio-Workflow-Success-Payloads.md",
                (
                    "spio --json build --manifest-path <path> ...",
                    "`workflow_success_payloads v1`",
                    "`receipt_path`",
                    "`diagnostics_path`",
                    "captured `stdout`",
                    "captured `stderr`",
                ),
            ),
        ),
    ),
    ContractRule(
        key="spio.supporting_json_success",
        summary="supporting spio JSON success commands stay aligned for source/deploy/toolchain flows",
        docs=(
            DocRule(
                "styio-nightly/docs/plans/Styio-Ecosystem-CLI-Contract-Matrix.md",
                (
                    "### 3.5 `spio --json fetch/vendor/pack/publish`",
                    "### 3.6 `spio --json tool install/use/pin`",
                    "supporting commands 成功时也必须写稳定 JSON object",
                ),
            ),
            DocRule(
                "styio-spio/docs/governance/Spio-CLI-Contract.md",
                (
                    "spio --json fetch --manifest-path path/to/spio.toml ...",
                    "spio --json tool install --styio-bin /path/to/styio",
                    "supporting internal commands invoked through `spio --json fetch/vendor/pack/publish/tool install/tool use/tool pin`",
                    "must also return one stable JSON success object on stdout",
                ),
            ),
            DocRule(
                "styio-view/docs/for-spio/Spio-Workflow-Success-Payloads.md",
                (
                    "spio --json fetch --manifest-path <path> ...",
                    "spio --json tool install --styio-bin <path>",
                    "成功时仍必须：",
                ),
            ),
            DocRule(
                "styio-view/docs/for-spio/Spio-Toolchain-And-Registry-State.md",
                (
                    "`spio --json fetch --manifest-path <path>`",
                    "`spio --json pack --manifest-path <path>`",
                    "`spio --json tool install --styio-bin <path>`",
                ),
            ),
        ),
    ),
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="ecosystem-cli-doc-gate.py")
    parser.add_argument(
        "--workspace-root",
        type=Path,
        default=DEFAULT_WORKSPACE_ROOT,
        help="workspace root that should contain styio-nightly, styio-spio, and styio-view",
    )
    parser.add_argument(
        "--require-workspace",
        action="store_true",
        help="fail instead of skipping when sibling repos are unavailable",
    )
    parser.add_argument("--json", action="store_true", help="emit machine-readable summary")
    return parser


def missing_repos(workspace_root: Path) -> List[str]:
    missing: List[str] = []
    for repo in REQUIRED_REPOS:
        if not (workspace_root / repo).is_dir():
            missing.append(repo)
    return missing


def check_doc_rule(workspace_root: Path, rule: DocRule) -> dict:
    path = workspace_root / rule.path
    result = {
        "path": rule.path,
        "exists": path.is_file(),
        "missing_needles": [],
        "ok": False,
    }
    if not path.is_file():
        return result
    text = path.read_text(encoding="utf-8")
    result["missing_needles"] = [needle for needle in rule.needles if needle not in text]
    result["ok"] = len(result["missing_needles"]) == 0
    return result


def check_contract(workspace_root: Path, contract: ContractRule) -> dict:
    docs = [check_doc_rule(workspace_root, rule) for rule in contract.docs]
    ok = all(doc["ok"] for doc in docs)
    return {
        "key": contract.key,
        "summary": contract.summary,
        "ok": ok,
        "docs": docs,
    }


def print_human(payload: dict) -> None:
    if payload.get("skipped"):
        print(f"[SKIP] {payload['reason']}")
        return
    for contract in payload["contracts"]:
        status = "OK" if contract["ok"] else "FAIL"
        print(f"[{status}] {contract['key']}: {contract['summary']}")
        if contract["ok"]:
            continue
        for doc in contract["docs"]:
            if doc["ok"]:
                continue
            if not doc["exists"]:
                print(f"  - missing file: {doc['path']}")
                continue
            print(f"  - {doc['path']}")
            for needle in doc["missing_needles"]:
                print(f"    missing: {needle}")
    print(
        "ecosystem CLI doc gate "
        + ("passed" if payload["ok"] else "failed")
        + f" ({len(payload['contracts'])} contract groups)"
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    workspace_root = args.workspace_root.resolve()
    missing = missing_repos(workspace_root)

    if missing:
        payload = {
            "ok": not args.require_workspace,
            "skipped": not args.require_workspace,
            "workspace_root": str(workspace_root),
            "reason": "missing sibling repositories: " + ", ".join(missing),
            "contracts": [],
        }
        if args.json:
            print(json.dumps(payload, sort_keys=True))
        else:
            print_human(payload)
        return 0 if payload["ok"] else 1

    contracts = [check_contract(workspace_root, contract) for contract in CONTRACT_RULES]
    payload = {
        "ok": all(contract["ok"] for contract in contracts),
        "skipped": False,
        "workspace_root": str(workspace_root),
        "reason": "",
        "contracts": contracts,
    }
    if args.json:
        print(json.dumps(payload, sort_keys=True))
    else:
        print_human(payload)
    return 0 if payload["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
