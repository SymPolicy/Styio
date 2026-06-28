#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import sys
import uuid
from pathlib import Path
from typing import Any


STATUSES = {"pending", "in_progress", "blocked", "completed", "skipped"}
TRANSITIONS = {
    "pending": {"pending", "in_progress", "blocked", "skipped"},
    "in_progress": {"in_progress", "completed", "blocked", "skipped"},
    "blocked": {"blocked", "in_progress", "skipped"},
    "completed": {"completed"},
    "skipped": {"skipped"},
}
UUID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
)


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise ValueError(f"missing JSON file: {path}") from None
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON in {path}: {exc}") from None


def is_uuid(value: object) -> bool:
    return isinstance(value, str) and UUID_RE.match(value) is not None


def require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def require_string(obj: dict[str, Any], key: str, label: str) -> str:
    value = obj.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label}.{key} must be a non-empty string")
    return value


def require_string_list(obj: dict[str, Any], key: str, label: str) -> list[str]:
    value = obj.get(key)
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ValueError(f"{label}.{key} must be a list of strings")
    return value


def validate_status(value: Any, label: str) -> str:
    if value not in STATUSES:
        raise ValueError(f"{label}.status must be one of {sorted(STATUSES)}")
    return str(value)


def validate_acceptance_criteria(node: dict[str, Any], label: str) -> None:
    criteria = node.get("acceptance_criteria")
    if not isinstance(criteria, list) or not criteria:
        raise ValueError(f"{label}.acceptance_criteria must be a non-empty list")
    for index, criterion in enumerate(criteria):
        item_label = f"{label}.acceptance_criteria[{index}]"
        if not isinstance(criterion, dict):
            raise ValueError(f"{item_label} must be an object")
        if not isinstance(criterion.get("checked"), bool):
            raise ValueError(f"{item_label}.checked must be a boolean")
        text = criterion.get("text")
        if not isinstance(text, str) or not text.strip():
            raise ValueError(f"{item_label}.text must be a non-empty string")


def validate_node(node: Any, index: int, seen_ids: dict[str, str]) -> dict[str, Any]:
    label = f"node[{index}]"
    obj = require_object(node, label)
    node_id = require_string(obj, "id", label)
    if not is_uuid(node_id):
        raise ValueError(f"{label}.id is not a canonical UUID: {node_id}")
    if node_id in seen_ids:
        raise ValueError(f"duplicate node id: {node_id}")
    status = validate_status(obj.get("status"), label)
    prerequisites = require_string_list(obj, "prerequisites", label)
    platform = require_string(obj, "platform", label)
    difficulty = require_string(obj, "difficulty", label)
    goal = require_string(obj, "goal", label)
    description = require_string(obj, "description", label)
    next_nodes = require_string_list(obj, "next", label)
    commit = require_object(obj.get("commit"), f"{label}.commit")
    require_string(commit, "repository", f"{label}.commit")
    require_string(commit, "message", f"{label}.commit")
    require_string(commit, "target", f"{label}.commit")
    validate_acceptance_criteria(obj, label)

    for prereq in prerequisites:
        if prereq not in seen_ids:
            raise ValueError(f"{label}.prerequisites references missing earlier node: {prereq}")

    seen_ids[node_id] = status
    return {
        "id": node_id,
        "status": status,
        "prerequisites": prerequisites,
        "platform": platform,
        "difficulty": difficulty,
        "goal": goal,
        "description": description,
        "next": next_nodes,
        "acceptance_criteria": obj["acceptance_criteria"],
    }


def validate_checkpoints(path: Path) -> list[dict[str, Any]]:
    data = load_json(path)
    if not isinstance(data, list):
        raise ValueError(f"{path} must contain a top-level array")

    seen_ids: dict[str, str] = {}
    nodes: list[dict[str, Any]] = []
    in_progress = 0
    for index, raw_node in enumerate(data):
        node = validate_node(raw_node, index, seen_ids)
        nodes.append(node)
        if node["status"] == "in_progress":
            in_progress += 1
            for prereq in node["prerequisites"]:
                if seen_ids[prereq] != "completed":
                    raise ValueError(
                        f"node[{index}] is in_progress but prerequisite is not completed: {prereq}"
                    )
        if node["status"] == "completed":
            for prereq in node["prerequisites"]:
                if seen_ids[prereq] != "completed":
                    raise ValueError(
                        f"node[{index}] is completed but prerequisite is not completed: {prereq}"
                    )
            unchecked = [
                item["text"]
                for item in node["acceptance_criteria"]
                if not item["checked"]
            ]
            if unchecked:
                raise ValueError(
                    f"node[{index}] is completed with unchecked acceptance criteria: {unchecked[0]}"
                )
    if in_progress > 1:
        raise ValueError(f"{path} has more than one in_progress node")

    all_ids = {node["id"] for node in nodes}
    for index, node in enumerate(nodes):
        for next_id in node["next"]:
            if next_id not in all_ids:
                raise ValueError(f"node[{index}].next references missing node: {next_id}")
    return nodes


def validate_plan(plan: Any, index: int, workspace: Path, seen_ids: set[str]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    label = f"plan[{index}]"
    obj = require_object(plan, label)
    plan_id = require_string(obj, "id", label)
    if not is_uuid(plan_id):
        raise ValueError(f"{label}.id is not a canonical UUID: {plan_id}")
    if plan_id in seen_ids:
        raise ValueError(f"duplicate plan id: {plan_id}")
    seen_ids.add(plan_id)

    status = validate_status(obj.get("status"), label)
    directory = require_string(obj, "directory", label)
    checkpoints = require_string(obj, "checkpoints", label)
    require_string(obj, "title", label)
    require_string(obj, "goal", label)
    require_string(obj, "description", label)
    require_string_list(obj, "source_files", label)

    expected_checkpoints = Path(directory) / "Checkpoints.json"
    if Path(checkpoints).as_posix() != expected_checkpoints.as_posix():
        raise ValueError(
            f"{label}.checkpoints must point to <directory>/Checkpoints.json"
        )
    plan_dir = workspace / directory
    if not plan_dir.is_dir():
        raise ValueError(f"{label}.directory does not exist: {plan_dir}")
    checkpoint_path = workspace / checkpoints
    nodes = validate_checkpoints(checkpoint_path)

    terminal = {"completed", "skipped"}
    if status == "completed" and any(node["status"] not in terminal for node in nodes):
        raise ValueError(f"{label} is completed but has non-terminal nodes")
    if status == "blocked" and not any(node["status"] == "blocked" for node in nodes):
        raise ValueError(f"{label} is blocked but no referenced node is blocked")
    if status == "skipped" and any(node["status"] == "in_progress" for node in nodes):
        raise ValueError(f"{label} is skipped but a node is in_progress")

    return obj, nodes


def validate_manifest(path: Path) -> None:
    workspace = path if path.is_dir() else path.parent
    manifest_path = workspace / "Manifest.json" if path.is_dir() else path
    data = load_json(manifest_path)
    if not isinstance(data, list):
        raise ValueError(f"{manifest_path} must contain a top-level array")
    seen_ids: set[str] = set()
    for index, plan in enumerate(data):
        validate_plan(plan, index, workspace, seen_ids)


def validate_target(target: Path) -> None:
    if target.is_dir() or target.name == "Manifest.json":
        validate_manifest(target)
        return
    if target.name == "Checkpoints.json":
        validate_checkpoints(target)
        return
    raise ValueError("validate target must be a Better Plan workspace, Manifest.json, or Checkpoints.json")


def command_uuid(_args: argparse.Namespace) -> int:
    print(str(uuid.uuid4()))
    return 0


def command_transition(args: argparse.Namespace) -> int:
    current = args.current
    target = args.target
    if current not in STATUSES:
        print(f"invalid current status: {current}", file=sys.stderr)
        return 2
    if target not in STATUSES:
        print(f"invalid target status: {target}", file=sys.stderr)
        return 2
    if target not in TRANSITIONS[current]:
        print(f"invalid transition: {current} -> {target}", file=sys.stderr)
        return 1
    print(f"ok: {current} -> {target}")
    return 0


def command_validate(args: argparse.Namespace) -> int:
    try:
        validate_target(Path(args.path))
    except ValueError as exc:
        print(f"manifest validation failed: {exc}", file=sys.stderr)
        return 1
    print("manifest validation passed")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate Better Plan manifests and checkpoint graphs.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    uuid_parser = subparsers.add_parser("uuid", help="Generate a canonical UUID.")
    uuid_parser.set_defaults(func=command_uuid)

    transition_parser = subparsers.add_parser("transition", help="Check a status transition.")
    transition_parser.add_argument("current")
    transition_parser.add_argument("target")
    transition_parser.set_defaults(func=command_transition)

    validate_parser = subparsers.add_parser("validate", help="Validate a workspace or state file.")
    validate_parser.add_argument("path")
    validate_parser.set_defaults(func=command_validate)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
