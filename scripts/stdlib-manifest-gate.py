#!/usr/bin/env python3
"""Validate the Styio standard-library manifest."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


MODULE_RE = re.compile(r"^std(\.[a-z][a-z0-9_]*)+$")
ALLOWED_STATUS = {"active", "planned", "deferred"}
REQUIRED_MODULE_FIELDS = {
    "name",
    "status",
    "directory",
    "schema_marker",
    "contract",
    "version_status",
    "trim_policy",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--manifest", type=Path, default=Path("library/manifest.json"))
    return parser.parse_args()


def validate_manifest(repo_root: Path, manifest_path: Path) -> list[str]:
    errors: list[str] = []
    resolved_manifest = manifest_path
    if not resolved_manifest.is_absolute():
        resolved_manifest = repo_root / resolved_manifest
    if not resolved_manifest.is_file():
        return [f"stdlib manifest not found: {resolved_manifest}"]

    with resolved_manifest.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    if manifest.get("schema") != "styio.stdlib_manifest.v1":
        errors.append(f"unsupported stdlib manifest schema: {manifest.get('schema')}")
    if manifest.get("package") != "std":
        errors.append("stdlib manifest package must be 'std'")

    modules = manifest.get("modules")
    if not isinstance(modules, list) or not modules:
        errors.append("stdlib manifest must contain at least one module")
        return errors

    require_test_evidence_paths = (repo_root / "tests").exists()
    seen_names: set[str] = set()
    seen_markers: set[str] = set()
    for index, module in enumerate(modules):
        if not isinstance(module, dict):
            errors.append(f"module #{index} must be an object")
            continue
        missing = sorted(REQUIRED_MODULE_FIELDS - set(module))
        if missing:
            errors.append(f"module #{index} missing required fields: {', '.join(missing)}")
            continue

        name = module["name"]
        status = module["status"]
        marker = module["schema_marker"]
        if not isinstance(name, str) or not MODULE_RE.match(name):
            errors.append(f"invalid stdlib module name: {name!r}")
        elif name in seen_names:
            errors.append(f"duplicate stdlib module name: {name}")
        else:
            seen_names.add(name)

        if status not in ALLOWED_STATUS:
            errors.append(f"{name}: unsupported status {status!r}")
        if not isinstance(marker, str) or not marker.startswith("STYIO_STDLIB_"):
            errors.append(f"{name}: schema_marker must start with STYIO_STDLIB_")
        elif marker in seen_markers:
            errors.append(f"{name}: duplicate schema_marker {marker}")
        else:
            seen_markers.add(marker)

        directory = repo_root / module["directory"]
        if not directory.is_dir():
            errors.append(f"{name}: directory does not exist: {module['directory']}")
        elif not (directory / "README.md").is_file():
            errors.append(f"{name}: directory must contain README.md: {module['directory']}")

        if status == "active":
            source = module.get("source")
            if not isinstance(source, str) or not source:
                errors.append(f"{name}: active module must declare source")
            elif not (repo_root / source).is_file():
                errors.append(f"{name}: active source does not exist: {source}")
            tests = module.get("tests")
            if not isinstance(tests, list) or not tests:
                errors.append(f"{name}: active module must list test evidence paths")
            else:
                for test_path in tests:
                    if not isinstance(test_path, str):
                        errors.append(f"{name}: test evidence path must be a string: {test_path!r}")
                    elif require_test_evidence_paths and not (repo_root / test_path).exists():
                        errors.append(f"{name}: test evidence path does not exist: {test_path}")
        elif "source" in module:
            errors.append(f"{name}: non-active module must not declare source")

    return errors


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    errors = validate_manifest(repo_root, args.manifest)
    if errors:
        for error in errors:
            print(f"stdlib manifest gate failed: {error}", file=sys.stderr)
        return 1
    print("stdlib manifest gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
