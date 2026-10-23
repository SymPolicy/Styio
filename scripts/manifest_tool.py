#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


MANIFEST_SCHEMA = "better-plan.manifest/v3"
PLAN_SCHEMA = "better-plan.plan/v3"
CHECKPOINTS_SCHEMA = "better-plan.checkpoints/v3"
PLAN_CODE_PREFIX = "PLAN-"
TASK_CODE_PREFIX = "TASK-"
REQUIRED_PLAN_FILES = (
    "Plan.json",
    "Plan.md",
    "Checkpoints.json",
    "Design.md",
    "Design.pristine.md",
)


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise ValueError(f"missing JSON file: {path}") from None
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON in {path}: {exc}") from None


def require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def require_list(value: Any, label: str) -> list[Any]:
    if not isinstance(value, list):
        raise ValueError(f"{label} must be an array")
    return value


def require_string(obj: dict[str, Any], key: str, label: str) -> str:
    value = obj.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label}.{key} must be a non-empty string")
    return value


def require_code(value: str, prefix: str, label: str) -> None:
    suffix = value.removeprefix(prefix)
    if not value.startswith(prefix) or not suffix.isdigit() or len(suffix) != 3:
        raise ValueError(f"{label} must use {prefix}NNN form: {value}")


def safe_relative_path(value: str, label: str) -> Path:
    path = Path(value)
    if path.is_absolute() or not path.parts or ".." in path.parts:
        raise ValueError(f"{label} must be a workspace-relative path")
    return path


def task_codes(tasks: Any, label: str) -> list[str]:
    codes: list[str] = []
    for index, raw_task in enumerate(require_list(tasks, label)):
        task = require_object(raw_task, f"{label}[{index}]")
        code = require_string(task, "code", f"{label}[{index}]")
        require_code(code, TASK_CODE_PREFIX, f"{label}[{index}].code")
        if code in codes:
            raise ValueError(f"duplicate task code in {label}: {code}")
        codes.append(code)
    return codes


def validate_plan_entry(workspace: Path, raw_entry: Any, index: int) -> tuple[str, str]:
    label = f"manifest.plans[{index}]"
    entry = require_object(raw_entry, label)
    code = require_string(entry, "code", label)
    require_code(code, PLAN_CODE_PREFIX, f"{label}.code")
    directory = require_string(entry, "directory", label)
    require_string(entry, "title", label)
    plan_path = safe_relative_path(require_string(entry, "plan", label), f"{label}.plan")
    checkpoints_path = safe_relative_path(
        require_string(entry, "checkpoints", label), f"{label}.checkpoints"
    )

    plan_dir = safe_relative_path(directory, f"{label}.directory")
    if len(plan_dir.parts) != 1:
        raise ValueError(f"{label}.directory must name one direct workspace child")
    if plan_path != plan_dir / "Plan.json":
        raise ValueError(f"{label}.plan must be {directory}/Plan.json")
    if checkpoints_path != plan_dir / "Checkpoints.json":
        raise ValueError(f"{label}.checkpoints must be {directory}/Checkpoints.json")

    absolute_plan_dir = workspace / plan_dir
    if not absolute_plan_dir.is_dir():
        raise ValueError(f"missing plan directory: {plan_dir.as_posix()}")
    for filename in REQUIRED_PLAN_FILES:
        if not (absolute_plan_dir / filename).is_file():
            raise ValueError(f"missing v3 plan artifact: {(plan_dir / filename).as_posix()}")

    plan = require_object(load_json(workspace / plan_path), plan_path.as_posix())
    if plan.get("schema") != PLAN_SCHEMA:
        raise ValueError(f"{plan_path.as_posix()}.schema must be {PLAN_SCHEMA}")
    if plan.get("code") != code:
        raise ValueError(f"{plan_path.as_posix()}.code must match manifest code {code}")
    if plan.get("directory") != directory:
        raise ValueError(f"{plan_path.as_posix()}.directory must match manifest directory")
    require_string(plan, "phase", plan_path.as_posix())
    plan_spec = require_object(plan.get("spec"), f"{plan_path.as_posix()}.spec")
    plan_task_codes = task_codes(plan_spec.get("tasks"), f"{plan_path.as_posix()}.spec.tasks")
    if not plan_task_codes:
        raise ValueError(f"{plan_path.as_posix()}.spec.tasks must not be empty")

    checkpoints = require_object(
        load_json(workspace / checkpoints_path), checkpoints_path.as_posix()
    )
    if checkpoints.get("schema") != CHECKPOINTS_SCHEMA:
        raise ValueError(
            f"{checkpoints_path.as_posix()}.schema must be {CHECKPOINTS_SCHEMA}"
        )
    if checkpoints.get("plan") != code:
        raise ValueError(f"{checkpoints_path.as_posix()}.plan must match {code}")
    checkpoint_task_codes = task_codes(
        checkpoints.get("tasks"), f"{checkpoints_path.as_posix()}.tasks"
    )
    if checkpoint_task_codes != plan_task_codes:
        raise ValueError(
            f"{checkpoints_path.as_posix()}.tasks must match Plan.json task order"
        )

    sealed = require_object(plan.get("lifecycle"), f"{plan_path.as_posix()}.lifecycle").get(
        "sealed"
    )
    if sealed is not None:
        sealed_obj = require_object(sealed, f"{plan_path.as_posix()}.lifecycle.sealed")
        if checkpoints.get("revision") != sealed_obj.get("revision"):
            raise ValueError(f"{checkpoints_path.as_posix()}.revision must match sealed revision")
        if checkpoints.get("semantic_digest") != sealed_obj.get("semantic_digest"):
            raise ValueError(
                f"{checkpoints_path.as_posix()}.semantic_digest must match sealed digest"
            )

    return code, directory


def validate_workspace(target: Path) -> None:
    workspace = target if target.is_dir() else target.parent
    manifest_path = workspace / "Manifest.json" if target.is_dir() else target
    if manifest_path.name != "Manifest.json":
        raise ValueError("validate target must be a v3 plan workspace or its Manifest.json")

    manifest = require_object(load_json(manifest_path), manifest_path.as_posix())
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise ValueError(f"{manifest_path}.schema must be {MANIFEST_SCHEMA}")
    plans = require_list(manifest.get("plans"), f"{manifest_path}.plans")
    if not plans:
        raise ValueError(f"{manifest_path}.plans must not be empty")

    seen_codes: set[str] = set()
    seen_directories: set[str] = set()
    for index, entry in enumerate(plans):
        code, directory = validate_plan_entry(workspace, entry, index)
        if code in seen_codes:
            raise ValueError(f"duplicate manifest plan code: {code}")
        if directory in seen_directories:
            raise ValueError(f"duplicate manifest plan directory: {directory}")
        seen_codes.add(code)
        seen_directories.add(directory)


def command_validate(args: argparse.Namespace) -> int:
    try:
        validate_workspace(Path(args.path))
    except ValueError as exc:
        print(f"manifest validation failed: {exc}", file=sys.stderr)
        return 1
    print("manifest validation passed")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate a Better Plan v3 workspace.")
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate_parser = subparsers.add_parser("validate", help="Validate v3 workspace structure.")
    validate_parser.add_argument("path")
    validate_parser.set_defaults(func=command_validate)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
