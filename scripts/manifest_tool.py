#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import sys
import uuid
from pathlib import Path
from typing import Any


STATUSES = {"pending", "in_progress", "blocked", "deferred", "completed", "skipped"}
TRANSITIONS = {
    "pending": {"pending", "in_progress", "blocked", "deferred", "skipped"},
    "in_progress": {"in_progress", "pending", "completed", "blocked", "deferred", "skipped"},
    "blocked": {"blocked", "in_progress", "deferred", "skipped"},
    "deferred": {"deferred", "pending", "blocked", "skipped"},
    "completed": {"completed"},
    "skipped": {"skipped"},
}
ROLES = {
    "architecture_scaffold",
    "evidence",
    "final_validation",
    "group_design",
    "implementation",
    "milestone_gate",
    "product_requirements",
    "validation_matrix",
}
PLATFORMS = {"any", "linux", "macos", "windows"}
DIFFICULTIES = {"routine", "standard", "complex", "critical"}
VERIFICATION_PROFILES = {"code", "hybrid", "visual"}
CAPABILITY_KINDS = {
    "capability",
    "component",
    "domain",
    "feature",
    "interface",
    "module",
    "repository",
    "service",
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


def project_root_for(workspace: Path) -> Path:
    for candidate in (workspace, *workspace.parents):
        if (candidate / ".git").exists():
            return candidate
    raise ValueError("Better Plan workspace must be inside a Git repository")


def validate_sources(
    sources: list[str], label: str, project_root: Path, workspace: Path
) -> None:
    for index, source in enumerate(sources):
        source_path = Path(source)
        if source_path.is_absolute() or ".." in source_path.parts:
            raise ValueError(f"{label}.source_files[{index}] must be repository-relative")
        if not (project_root / source_path).exists() and not (workspace / source_path).exists():
            raise ValueError(f"{label}.source_files[{index}] does not exist: {source}")


def validate_capabilities(
    workspace: Path, project_root: Path, check_sources: bool
) -> None:
    path = workspace / "Capabilities.json"
    data = load_json(path)
    if not isinstance(data, list) or not data:
        raise ValueError(f"{path} must contain a non-empty top-level array")

    seen: set[str] = set()
    roots = 0
    for index, raw_capability in enumerate(data):
        label = f"capability[{index}]"
        capability = require_object(raw_capability, label)
        key = require_string(capability, "key", label)
        if key in seen:
            raise ValueError(f"duplicate capability key: {key}")
        parent = capability.get("parent")
        if parent is None:
            roots += 1
            if capability.get("kind") != "repository":
                raise ValueError(f"{label}.kind must be repository for the root capability")
        elif not isinstance(parent, str) or parent not in seen:
            raise ValueError(f"{label}.parent must reference an earlier capability")
        kind = require_string(capability, "kind", label)
        if kind not in CAPABILITY_KINDS:
            raise ValueError(f"{label}.kind must be one of {sorted(CAPABILITY_KINDS)}")
        if capability.get("basis") not in {"observed", "designed"}:
            raise ValueError(f"{label}.basis must be observed or designed")
        if capability.get("disclosure") not in {"known", "examined"}:
            raise ValueError(f"{label}.disclosure must be known or examined")
        if capability.get("touch") not in {"untouched", "in_scope", "modified"}:
            raise ValueError(f"{label}.touch must be untouched, in_scope, or modified")
        require_string(capability, "title", label)
        require_string(capability, "description", label)
        sources = require_string_list(capability, "source_files", label)
        if check_sources:
            validate_sources(sources, label, project_root, workspace)
        seen.add(key)
    if roots != 1:
        raise ValueError("Capabilities.json must contain exactly one root capability")


def validate_node(node: Any, index: int, seen_ids: dict[str, str]) -> dict[str, Any]:
    label = f"node[{index}]"
    obj = require_object(node, label)
    node_id = require_string(obj, "id", label)
    if not is_uuid(node_id):
        raise ValueError(f"{label}.id is not a canonical UUID: {node_id}")
    if node_id in seen_ids:
        raise ValueError(f"duplicate node id: {node_id}")
    status = validate_status(obj.get("status"), label)
    role = require_string(obj, "role", label)
    if role not in ROLES:
        raise ValueError(f"{label}.role must be one of {sorted(ROLES)}")
    prerequisites = require_string_list(obj, "prerequisites", label)
    platform = require_string(obj, "platform", label)
    if platform not in PLATFORMS:
        raise ValueError(f"{label}.platform must be one of {sorted(PLATFORMS)}")
    difficulty = require_string(obj, "difficulty", label)
    if difficulty not in DIFFICULTIES:
        raise ValueError(f"{label}.difficulty must be one of {sorted(DIFFICULTIES)}")
    verification_profile = require_string(obj, "verification_profile", label)
    if verification_profile not in VERIFICATION_PROFILES:
        raise ValueError(
            f"{label}.verification_profile must be one of {sorted(VERIFICATION_PROFILES)}"
        )
    goal = require_string(obj, "goal", label)
    description = require_string(obj, "description", label)
    next_nodes = require_string_list(obj, "next", label)
    commit = require_object(obj.get("commit"), f"{label}.commit")
    require_string(commit, "repository", f"{label}.commit")
    require_string(commit, "message", f"{label}.commit")
    require_string(commit, "target", f"{label}.commit")
    validate_acceptance_criteria(obj, label)

    seen_ids[node_id] = status
    return {
        "id": node_id,
        "status": status,
        "role": role,
        "prerequisites": prerequisites,
        "platform": platform,
        "difficulty": difficulty,
        "verification_profile": verification_profile,
        "goal": goal,
        "description": description,
        "next": next_nodes,
        "acceptance_criteria": obj["acceptance_criteria"],
    }


def validate_checkpoints(
    path: Path, seen_ids: dict[str, str] | None = None
) -> list[dict[str, Any]]:
    data = load_json(path)
    if not isinstance(data, list):
        raise ValueError(f"{path} must contain a top-level array")

    all_seen_ids = seen_ids if seen_ids is not None else {}
    nodes: list[dict[str, Any]] = []
    in_progress = 0
    for index, raw_node in enumerate(data):
        node = validate_node(raw_node, index, all_seen_ids)
        nodes.append(node)
        if node["status"] == "in_progress":
            in_progress += 1
        if node["status"] == "completed":
            unchecked = [
                item["text"]
                for item in node["acceptance_criteria"]
                if not item["checked"]
            ]
            if unchecked:
                raise ValueError(
                    f"node[{index}] is completed with unchecked acceptance criteria: {unchecked[0]}"
                )
    if in_progress > 1 and any(
        node["status"] == "in_progress" and node["role"] != "implementation"
        for node in nodes
    ):
        raise ValueError(f"{path} has more than one in_progress node")
    return nodes


def validate_plan(
    plan: Any,
    index: int,
    workspace: Path,
    seen_plan_ids: set[str],
    seen_node_ids: dict[str, str],
    project_root: Path,
    check_sources: bool,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    label = f"plan[{index}]"
    obj = require_object(plan, label)
    plan_id = require_string(obj, "id", label)
    if not is_uuid(plan_id):
        raise ValueError(f"{label}.id is not a canonical UUID: {plan_id}")
    if plan_id in seen_plan_ids:
        raise ValueError(f"duplicate plan id: {plan_id}")
    seen_plan_ids.add(plan_id)

    status = validate_status(obj.get("status"), label)
    directory = require_string(obj, "directory", label)
    checkpoints = require_string(obj, "checkpoints", label)
    require_string(obj, "title", label)
    require_string(obj, "purpose", label)
    require_string(obj, "goal", label)
    require_string(obj, "description", label)
    source_files = require_string_list(obj, "source_files", label)
    if check_sources:
        validate_sources(source_files, label, project_root, workspace)

    expected_checkpoints = Path(directory) / "Checkpoints.json"
    if Path(checkpoints).as_posix() != expected_checkpoints.as_posix():
        raise ValueError(
            f"{label}.checkpoints must point to <directory>/Checkpoints.json"
        )
    plan_dir = workspace / directory
    if not plan_dir.is_dir():
        raise ValueError(f"{label}.directory does not exist: {plan_dir}")
    checkpoint_path = workspace / checkpoints
    nodes = validate_checkpoints(checkpoint_path, seen_node_ids)

    terminal = {"completed", "skipped"}
    if status == "completed" and any(node["status"] not in terminal for node in nodes):
        raise ValueError(f"{label} is completed but has non-terminal nodes")
    if status == "blocked" and not any(node["status"] == "blocked" for node in nodes):
        raise ValueError(f"{label} is blocked but no referenced node is blocked")
    if status == "skipped" and any(node["status"] == "in_progress" for node in nodes):
        raise ValueError(f"{label} is skipped but a node is in_progress")

    return obj, nodes


def validate_manifest(path: Path, check_sources: bool = False) -> None:
    workspace = path if path.is_dir() else path.parent
    manifest_path = workspace / "Manifest.json" if path.is_dir() else path
    data = load_json(manifest_path)
    if not isinstance(data, list):
        raise ValueError(f"{manifest_path} must contain a top-level array")
    project_root = project_root_for(workspace)
    validate_capabilities(workspace, project_root, check_sources)
    seen_plan_ids: set[str] = set()
    seen_node_ids: dict[str, str] = {}
    all_nodes: list[dict[str, Any]] = []
    for index, plan in enumerate(data):
        _, nodes = validate_plan(
            plan,
            index,
            workspace,
            seen_plan_ids,
            seen_node_ids,
            project_root,
            check_sources,
        )
        all_nodes.extend(nodes)

    for index, node in enumerate(all_nodes):
        for prereq in node["prerequisites"]:
            if prereq not in seen_node_ids:
                raise ValueError(f"node[{index}].prerequisites references missing node: {prereq}")
            if node["status"] in {"in_progress", "completed"} and seen_node_ids[prereq] != "completed":
                raise ValueError(
                    f"node[{index}] is {node['status']} but prerequisite is not completed: {prereq}"
                )
        for next_id in node["next"]:
            if next_id not in seen_node_ids:
                raise ValueError(f"node[{index}].next references missing node: {next_id}")

    visiting: set[str] = set()
    visited: set[str] = set()
    prerequisites_by_id = {node["id"]: node["prerequisites"] for node in all_nodes}

    def visit(node_id: str) -> None:
        if node_id in visited:
            return
        if node_id in visiting:
            raise ValueError(f"checkpoint dependency cycle includes node: {node_id}")
        visiting.add(node_id)
        for prerequisite in prerequisites_by_id[node_id]:
            visit(prerequisite)
        visiting.remove(node_id)
        visited.add(node_id)

    for node_id in prerequisites_by_id:
        visit(node_id)


def validate_target(target: Path, check_sources: bool = False) -> None:
    if target.is_dir() or target.name == "Manifest.json":
        validate_manifest(target, check_sources)
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
        validate_target(Path(args.path), args.check_sources)
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
    validate_parser.add_argument(
        "--check-sources",
        action="store_true",
        help="Require every repository-relative source file to exist.",
    )
    validate_parser.set_defaults(func=command_validate)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
