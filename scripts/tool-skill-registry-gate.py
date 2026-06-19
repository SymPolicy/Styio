#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import json
import shlex
import sys
import tomllib
from datetime import date
from pathlib import Path
from typing import Any, Iterable, Sequence

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "workflows/TOOL-SKILL-REGISTRY-GATE.toml"
WORKFLOW_REGISTRY = ROOT / "workflows/workflows.toml"
SKILLS_DIR = ROOT / "workflows/skills"
SCHEDULER = ROOT / "scripts/workflow-scheduler.py"


def load_toml(path: Path) -> dict[str, Any]:
    with path.open("rb") as handle:
        return tomllib.load(handle)


def iso_date(value: Any) -> str:
    if isinstance(value, date):
        return value.isoformat()
    if isinstance(value, str):
        return value
    return str(value)


def date_on_or_after(value: Any, minimum: str) -> bool:
    try:
        return date.fromisoformat(iso_date(value)) >= date.fromisoformat(minimum)
    except ValueError:
        return False


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def require_unique(entries: Sequence[dict[str, Any]], field: str, label: str, errors: list[str]) -> dict[str, dict[str, Any]]:
    output: dict[str, dict[str, Any]] = {}
    for entry in entries:
        value = entry.get(field)
        if not isinstance(value, str) or not value:
            errors.append(f"{label} is missing {field}: {entry!r}")
            continue
        if value in output:
            errors.append(f"duplicate {label} {field}: {value}")
            continue
        output[value] = entry
    return output


def scan_tool_files() -> set[str]:
    output: set[str] = set()
    for root_name in ("scripts", "benchmark"):
        root = ROOT / root_name
        if not root.exists():
            continue
        for path in sorted(root.iterdir()):
            if path.is_file() and path.suffix in {".py", ".sh"}:
                output.add(rel(path))
    return output


def first_line(path: Path) -> str:
    with path.open("r", encoding="utf-8") as handle:
        return handle.readline().strip()


def validate_tool_file(entry: dict[str, Any], review_date: str, forbidden_markers: Sequence[str], errors: list[str]) -> None:
    tool_path_raw = entry.get("path")
    if not isinstance(tool_path_raw, str) or not tool_path_raw:
        errors.append(f"tool {entry.get('id', '<unknown>')} is missing path")
        return

    path = ROOT / tool_path_raw
    if not path.is_file():
        errors.append(f"registered tool is missing: {tool_path_raw}")
        return

    if not date_on_or_after(entry.get("last_reviewed"), review_date):
        errors.append(f"tool {entry.get('id')} last_reviewed is older than {review_date}: {tool_path_raw}")

    line = first_line(path)
    if path.suffix == ".py" and line != "#!/usr/bin/env python3":
        errors.append(f"python tool must start with python3 shebang: {tool_path_raw}")
    if path.suffix == ".sh" and line != "#!/usr/bin/env bash":
        errors.append(f"shell tool must start with bash shebang: {tool_path_raw}")

    text = path.read_text(encoding="utf-8")
    for marker in forbidden_markers:
        if marker in text:
            errors.append(f"active tool contains forbidden marker '{marker}': {tool_path_raw}")


def validate_tools(registry: dict[str, Any], errors: list[str]) -> dict[str, dict[str, Any]]:
    review_date = iso_date(registry.get("current_state_review_date"))
    forbidden_markers = [str(item) for item in registry.get("forbidden_active_markers", [])]
    forbidden_paths = {str(item) for item in registry.get("forbidden_tool_paths", [])}

    tools = registry.get("tool", [])
    if not isinstance(tools, list):
        errors.append("registry field [[tool]] must be a list")
        tools = []
    by_id = require_unique(tools, "id", "tool", errors)
    by_path = require_unique(tools, "path", "tool", errors)

    scanned = scan_tool_files()
    registered_paths = set(by_path)
    for path in sorted(scanned - registered_paths):
        errors.append(f"tool file is not registered: {path}")
    for path in sorted(registered_paths - scanned):
        errors.append(f"registered tool path is not an active script/benchmark tool: {path}")
    for path in sorted(forbidden_paths):
        if (ROOT / path).exists():
            errors.append(f"forbidden retired tool path exists: {path}")
        if path in registered_paths:
            errors.append(f"forbidden retired tool path is registered: {path}")

    for entry in tools:
        validate_tool_file(entry, review_date, forbidden_markers, errors)

    return by_id


def validate_workflow_registry(registry: dict[str, Any], errors: list[str]) -> None:
    workflow_registry = load_toml(WORKFLOW_REGISTRY)
    workflows = workflow_registry.get("workflow", [])
    workflow_ids = {item.get("id") for item in workflows if isinstance(item, dict)}
    if registry.get("id") not in workflow_ids:
        errors.append(f"{WORKFLOW_REGISTRY.relative_to(ROOT)} does not register workflow {registry.get('id')}")

    skills = workflow_registry.get("skill", [])
    registered_skill_ids = {item.get("id") for item in skills if isinstance(item, dict)}
    required_skill_ids = set(registry.get("required_skill_ids", []))
    if registered_skill_ids != required_skill_ids:
        errors.append(
            "workflows/workflows.toml skill set does not match current registry: "
            f"registered={sorted(registered_skill_ids)} required={sorted(required_skill_ids)}"
        )


def validate_skills(registry: dict[str, Any], errors: list[str]) -> None:
    review_date = iso_date(registry.get("current_state_review_date"))
    required_skill_ids = set(str(item) for item in registry.get("required_skill_ids", []))
    forbidden_markers = [str(item) for item in registry.get("forbidden_active_markers", [])]

    actual_skill_files = sorted(SKILLS_DIR.glob("*/skill.toml"))
    actual_skill_ids: set[str] = set()
    workflow_skill_refs = {
        str(data.get("skill"))
        for path in (ROOT / "workflows").glob("*.toml")
        if path.name not in {"workflows.toml", "TOOL-SKILL-REGISTRY-GATE.toml"}
        for data in [load_toml(path)]
        if isinstance(data.get("skill"), str)
    }

    for skill_file in actual_skill_files:
        data = load_toml(skill_file)
        skill = data.get("skill", {})
        if not isinstance(skill, dict):
            errors.append(f"missing [skill] table: {rel(skill_file)}")
            continue
        name = skill.get("name")
        if not isinstance(name, str) or not name:
            errors.append(f"skill is missing name: {rel(skill_file)}")
            continue
        actual_skill_ids.add(name)
        if name not in required_skill_ids:
            errors.append(f"unregistered repo-local skill remains active: {name}")
        if not date_on_or_after(skill.get("last_updated"), review_date):
            errors.append(f"skill {name} last_updated is older than {review_date}")
        if skill_file.relative_to(ROOT / "workflows").as_posix() not in workflow_skill_refs:
            errors.append(f"active skill is not referenced by a workflow: {rel(skill_file)}")
        for reference in skill.get("references", []):
            if not isinstance(reference, str):
                errors.append(f"skill {name} has non-string reference: {reference!r}")
                continue
            if not (skill_file.parent / reference).is_file():
                errors.append(f"skill {name} reference is missing: {reference}")

        text = skill_file.read_text(encoding="utf-8")
        for marker in forbidden_markers:
            if marker in text:
                errors.append(f"active skill contains forbidden marker '{marker}': {rel(skill_file)}")

    if actual_skill_ids != required_skill_ids:
        errors.append(
            "active skill directories do not match current registry: "
            f"actual={sorted(actual_skill_ids)} required={sorted(required_skill_ids)}"
        )


def validate_module_coverage(
    registry: dict[str, Any],
    tools_by_id: dict[str, dict[str, Any]],
    errors: list[str],
) -> None:
    modules = registry.get("module_coverage", [])
    if not isinstance(modules, list):
        errors.append("registry field [[module_coverage]] must be a list")
        return

    seen_modules: set[str] = set()
    for entry in modules:
        module = entry.get("module")
        if not isinstance(module, str) or not module:
            errors.append(f"module coverage entry is missing module: {entry!r}")
            continue
        if module in seen_modules:
            errors.append(f"duplicate module coverage entry: {module}")
        seen_modules.add(module)

        runbook = entry.get("runbook")
        if not isinstance(runbook, str) or not (ROOT / runbook).is_file():
            errors.append(f"module {module} has missing runbook: {runbook}")

        tool_ids = entry.get("tool_ids", [])
        if not isinstance(tool_ids, list) or not tool_ids:
            errors.append(f"module {module} must list at least one maintenance tool")
            continue

        coverage_tools = 0
        for tool_id in tool_ids:
            if not isinstance(tool_id, str) or tool_id not in tools_by_id:
                errors.append(f"module {module} references unknown tool: {tool_id}")
                continue
            if tools_by_id[tool_id].get("kind") == "support-library":
                errors.append(f"module {module} cannot count support library as maintenance tool: {tool_id}")
                continue
            coverage_tools += 1
        if coverage_tools == 0:
            errors.append(f"module {module} has no usable maintenance tool")

        command = entry.get("maintenance_command")
        if not isinstance(command, str) or not command.strip():
            errors.append(f"module {module} is missing maintenance_command")
            continue
        try:
            parts = shlex.split(command)
        except ValueError as exc:
            errors.append(f"module {module} maintenance_command is not shell-parseable: {exc}")
            continue
        for part in parts:
            if part.startswith(("./", "scripts/", "benchmark/")):
                candidate = (ROOT / part.removeprefix("./")).resolve()
                try:
                    candidate.relative_to(ROOT)
                except ValueError:
                    continue
                if candidate.suffix in {".py", ".sh"} and not candidate.is_file():
                    errors.append(f"module {module} command references missing tool: {part}")


def import_scheduler() -> Any:
    spec = importlib.util.spec_from_file_location("workflow_scheduler_for_registry_gate", SCHEDULER)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load workflow scheduler")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def command_script_paths(command: Sequence[str]) -> Iterable[str]:
    for part in command:
        rendered = part.replace("{base}", "BASE").replace("{range}", "BASE..HEAD")
        if rendered.startswith(("scripts/", "benchmark/")) and Path(rendered).suffix in {".py", ".sh"}:
            yield rendered


def validate_scheduler(tools_by_path: dict[str, dict[str, Any]], errors: list[str]) -> None:
    try:
        scheduler = import_scheduler()
    except Exception as exc:
        errors.append(f"cannot import workflow scheduler: {exc}")
        return

    scheduler_errors = scheduler.validate_registry()
    if scheduler_errors:
        errors.extend(f"workflow scheduler registry error: {error}" for error in scheduler_errors)

    tool_keys = {tool.key for tool in scheduler.TOOLS}
    if "tool-skill-registry" not in tool_keys:
        errors.append("workflow scheduler does not register tool-skill-registry tool")

    required_profiles = {
        "delivery-checkpoint",
        "delivery-staged",
        "delivery-push",
        "syntax-local",
        "ci-prebuild",
    }
    for profile in scheduler.PROFILES:
        if profile.key in required_profiles and "tool-skill-registry" not in profile.tools:
            errors.append(f"profile {profile.key} does not run tool-skill-registry")

    for tool in scheduler.TOOLS:
        for path in command_script_paths(tool.command):
            if path not in tools_by_path:
                errors.append(f"scheduler tool {tool.key} references unregistered script: {path}")


def run_check() -> tuple[int, dict[str, Any]]:
    errors: list[str] = []
    registry = load_toml(REGISTRY)

    tools_by_id = validate_tools(registry, errors)
    tools_by_path = {
        str(entry.get("path")): entry
        for entry in tools_by_id.values()
        if isinstance(entry.get("path"), str)
    }

    validate_workflow_registry(registry, errors)
    validate_skills(registry, errors)
    validate_module_coverage(registry, tools_by_id, errors)
    validate_scheduler(tools_by_path, errors)

    payload = {
        "ok": not errors,
        "tool_count": len(tools_by_id),
        "skill_count": len(registry.get("required_skill_ids", [])),
        "module_count": len(registry.get("module_coverage", [])),
        "errors": errors,
    }
    return (0 if not errors else 1), payload


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate current repo-local skills and maintenance tools.")
    parser.add_argument("--format", choices=["text", "json"], default="text")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    status, payload = run_check()
    if args.format == "json":
        print(json.dumps(payload, indent=2, sort_keys=True))
        return status

    if payload["ok"]:
        print(
            "tool-skill registry gate passed: "
            f"{payload['tool_count']} tools, {payload['skill_count']} skills, "
            f"{payload['module_count']} module coverage entries"
        )
        return 0

    print("tool-skill registry gate failed:", file=sys.stderr)
    for error in payload["errors"]:
        print(f"  - {error}", file=sys.stderr)
    return status


if __name__ == "__main__":
    sys.exit(main())
