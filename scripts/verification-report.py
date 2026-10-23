#!/usr/bin/env python3
"""Advisory file discovery and CI evidence reporting; never run build/test commands."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
from collections import Counter
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TRANSLATION_UNITS = {".c", ".cc", ".cpp", ".cxx"}
HEADERS = {".h", ".hh", ".hpp", ".hxx", ".inc", ".in"}
ROUTES = {
    "src": "compiler targets and affected CTest suites",
    "tests": "CTest registration, labels and fixture consumers",
    "scripts": "process gates and focused script tests",
    "docs": "documentation audit",
    "workflows": "workflow/skill registry and documentation audit",
    ".github": "CI configuration and repository hygiene",
    "cmake": "CMake configure and affected build targets",
    "configs": "configuration consumers and affected targets",
    "grammar": "grammar generation and IDE tests",
    "library": "standard-library manifest and consumer tests",
    "example": "example consumers; execution is not implied",
    "templates": "template consumers; execution is not implied",
    "benchmark": "optional external benchmark integration",
    "share": "packaging consumers",
    ".docker": "container environment configuration",
    ".devcontainer": "development environment configuration",
    ".githooks": "local hook and delivery process checks",
}
KNOWN_SUFFIXES = TRANSLATION_UNITS | HEADERS | {
    ".md", ".py", ".sh", ".cmake", ".styio", ".toml", ".json", ".jsonl",
    ".yml", ".yaml", ".txt", ".out", ".err", ".csv", ".tsv", ".html",
    ".css", ".js", ".svg", ".png", ".ico", ".g4", ".scm", ".ps1", ".seed", ".dict",
}
ROOT_CONFIG = {".clang-format", ".gitignore", ".nvmrc", ".python-version", "LICENSE", "NOTICE"}


def tracked_files(root: Path) -> list[str]:
    result = subprocess.run(["git", "ls-files", "-z"], cwd=root, check=True, capture_output=True)
    return sorted(set(os.fsdecode(p) for p in result.stdout.split(b"\0") if p))


def owner_rules(root: Path):
    spec = importlib.util.spec_from_file_location("report_team_rules", root / "scripts/team-docs-gate.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module.TEAM_RULES


def compilation_evidence(root: Path, database: Path | None) -> tuple[dict, list[str]]:
    if database is None or not database.is_file():
        return {}, ["Compilation database unavailable; target membership and object presence are unverified."]
    evidence: dict[str, list[dict]] = {}
    for entry in json.loads(database.read_text()):
        directory = Path(entry["directory"])
        source = Path(entry["file"])
        source = (source if source.is_absolute() else directory / source).resolve()
        try:
            relative = source.relative_to(root.resolve()).as_posix()
        except ValueError:
            continue  # Downloaded/generated external inputs are outside tracked-file scope.
        args = entry.get("arguments") or shlex.split(entry.get("command", ""))
        output = entry.get("output", "")
        # The command's -o is relative to its working directory. Some CMake
        # generators express the optional output field relative to build root.
        if "-o" in args and args.index("-o") + 1 < len(args):
            output = args[args.index("-o") + 1]
        parts = Path(output).parts
        target = next((part[:-4] for part in parts if part.endswith(".dir")), None)
        obj = Path(output)
        obj = obj if obj.is_absolute() else directory / obj
        evidence.setdefault(relative, []).append({
            "target": target, "object_present": bool(output) and obj.is_file(),
        })
    return evidence, []


def classify(path: str, evidence: dict, database_available: bool, rules=()) -> dict:
    p = Path(path)
    route = ROUTES.get(p.parts[0]) if len(p.parts) > 1 else None
    if p.suffix == ".md":
        route = "documentation audit"
    elif p.name == "CMakeLists.txt":
        route = "CMake configure and affected build targets"
    elif path in ROOT_CONFIG:
        route = "repository configuration/hygiene"
    known_type = path.startswith(("tests/fuzz/corpus/", ".githooks/")) or p.suffix in KNOWN_SUFFIXES or p.name in ROOT_CONFIG or p.name in {"CMakeLists.txt", "Dockerfile"}
    row = {"path": path, "route": route if known_type else None,
           "owners": sorted({r.label for r in rules for prefix in r.prefixes if path == prefix or path.startswith(prefix) and prefix.endswith("/")}),
           "status": "routed" if route and known_type else "unclassified",
           "reason": "Routing identifies checks to inspect, not proof of test execution or behavioral coverage."}
    if p.suffix in TRANSLATION_UNITS:
        entries = evidence.get(path, [])
        row["targets"] = sorted({e["target"] for e in entries if e["target"]})
        row["compilation"] = entries  # Preserve partial/multi-target observations.
        if entries:
            row["status"] = "object-present" if any(e["object_present"] for e in entries) else "configured-no-object"
            row["reason"] = "CMake compilation database membership; object presence is not test coverage or freshness proof."
        elif path.startswith("tests/features/native_interop/native/"):
            row["status"] = "runtime-fixture-unverified"
            row["reason"] = "Candidate native-interop runtime fixture inferred from its directory; consumer reference and runtime compilation are unverified."
        else:
            row["status"] = "no-configured-target" if database_available else "target-unverified"
            row["reason"] = "No compile entry in this configuration; may be optional/platform-specific or omitted. Review, do not block."
        if path.startswith("grammar/tree-sitter-styio/src/"):
            row["reason"] += " Generated grammar source remains inventoried and target-checked."
    elif p.suffix in HEADERS:
        row["reason"] = "Header/include/generator input: consumed indirectly; no standalone translation-unit claim."
    return row


def inventory(root: Path, database: Path | None, build_outcome: str) -> dict:
    available = database is not None and database.is_file()
    try:
        evidence, warnings = compilation_evidence(root, database)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        evidence, warnings, available = {}, [f"Compilation evidence unreadable: {type(exc).__name__}; target membership unverified."], False
    rules = owner_rules(root)
    rows = [classify(path, evidence, available, rules) for path in tracked_files(root)]
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=root))
    return {"advisory": True, "revision": revision, "dirty": dirty, "build_outcome": build_outcome,
            "scope": "Git-tracked files, including staged additions; untracked/ignored files are not part of the candidate.",
            "warnings": warnings, "counts": dict(Counter(r["status"] for r in rows)), "files": rows}


def final_report(inventory_data: dict, results: dict, checks: list, jobs: list = ()) -> dict:
    reasons = {}
    for lane, result in results.items():
        outcome = result.get("result", "unknown") if isinstance(result, dict) else result
        if outcome == "skipped":
            reasons[lane] = "Not executed; inspect dependency/condition results. No passing evidence."
        elif outcome == "cancelled":
            reasons[lane] = "Run reported cancellation; no passing evidence."
    return {"advisory": True, "inventory": inventory_data, "lanes": results, "lane_reasons": reasons, "jobs": jobs,
            "observed_at": datetime.now(timezone.utc).isoformat(),
            "candidate": os.environ.get("STYIO_CANDIDATE", "unavailable"),
            "run_id": os.environ.get("GITHUB_RUN_ID", "local"),
            "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", "local"),
            "other_checks_snapshot": checks,
            "limitations": [
                "This report consumes existing results; it does not run or rerun tests.",
                "Directory/type routing is not a file-level behavioral coverage claim.",
                "Compilation membership/object evidence covers the Linux configuration only; object presence alone does not prove freshness or successful linking.",
                "Separate-workflow checks are a snapshot, not a wait or an assertion that pending/missing checks passed.",
                "Generated and vendor tracked files are still inventoried. Runtime native fixtures have an explicit reason; downloaded/ignored build dependencies are outside the Git candidate.",
                "Missing artifacts or report errors remain unverified; existing required CI determines merge eligibility.",
            ]}


def markdown(report: dict) -> str:
    data = report.get("inventory", report)
    lines = ["# CI verification report (advisory)", "", "No tests were rerun to produce this report.", ""]
    if "inventory" in report:
        lines += [f"- PR head / push candidate: {report.get('candidate', 'unavailable')}",
                  f"- Run / attempt: {report.get('run_id', 'unavailable')} / {report.get('run_attempt', 'unavailable')}",
                  f"- Snapshot time: {report.get('observed_at', 'unavailable')}", ""]
    for name, result in report.get("lanes", {}).items():
        value = result.get("result", "unknown") if isinstance(result, dict) else result
        reason = report.get("lane_reasons", {}).get(name, "")
        lines.append(f"- {name}: {value}" + (f" ({reason})" if reason else ""))
    lines += ["", f"- Inventory revision: {data.get('revision', 'unavailable')}",
              f"- Linux build step: {data.get('build_outcome', 'unverified')}",
              f"- Tracked files discovered: {len(data.get('files', []))}", ""]
    for status, count in sorted(data.get("counts", {}).items()):
        lines.append(f"- {status}: {count}")
    lines += ["", "## Findings", ""]
    warnings = data.get("warnings", []) + (report.get("warnings", []) if "inventory" in report else [])
    for warning in warnings:
        lines.append(f"- {warning}")
    findings = [r for r in data.get("files", []) if r["status"] in {"unclassified", "no-configured-target", "configured-no-object", "target-unverified", "runtime-fixture-unverified"}]
    for row in findings[:40]:
        path = row["path"].replace("`", "'").replace("\n", " ")
        lines.append(f"- `{path}`: {row['status']}")
    if len(findings) > 40:
        lines.append(f"- {len(findings) - 40} more findings in the JSON artifact")
    lines += ["", "## Existing job and step outcomes", ""]
    for job in report.get("jobs", []):
        lines.append(f"- {job.get('name')}: {job.get('conclusion') or job.get('status', 'unknown')}")
        for step in job.get("steps", []):
            lines.append(f"  - {step.get('name')}: {step.get('conclusion') or step.get('status', 'unknown')}")
    if not report.get("jobs"):
        lines.append("- Detailed step outcomes unavailable; lane results above remain separate evidence")
    lines += ["", "## Other check snapshot", ""]
    checks = report.get("other_checks_snapshot", [])
    for check in checks:
        lines.append(f"- {check.get('name', 'unknown')}: {check.get('conclusion') or check.get('status', 'unknown')}")
    if not checks:
        lines.append("- Separate-workflow check snapshot unavailable; consult the PR checks")
    lines += ["", "## Limits", ""] + [f"- {item}" for item in report.get("limitations", [])]
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("inventory", "final"))
    parser.add_argument("--compile-commands", type=Path)
    parser.add_argument("--build-outcome", default="unverified")
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--checks", type=Path)
    parser.add_argument("--jobs", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.mode == "inventory":
            report = inventory(ROOT, args.compile_commands, args.build_outcome)
        else:
            warnings = []

            def read_evidence(path, label, expected):
                try:
                    value = json.loads(path.read_text()) if path and path.is_file() else None
                    if not isinstance(value, expected):
                        raise ValueError("missing or unexpected shape")
                    if expected is list and not all(isinstance(item, dict) for item in value):
                        raise ValueError("unexpected entry shape")
                    if label == "Inventory artifact" and (
                        not isinstance(value.get("files", []), list)
                        or not all(isinstance(item, dict) and "path" in item and "status" in item for item in value.get("files", []))
                        or not isinstance(value.get("counts", {}), dict)
                    ):
                        raise ValueError("unexpected inventory shape")
                    return value
                except (OSError, ValueError, TypeError):
                    warnings.append(f"{label} unavailable or malformed; that evidence remains unverified.")
                    return expected()

            data = read_evidence(args.inventory, "Inventory artifact", dict)
            checks = read_evidence(args.checks, "Separate-workflow check snapshot", list)
            jobs = read_evidence(args.jobs, "Detailed job/step snapshot", list)
            try:
                results = json.loads(os.environ.get("STYIO_CI_RESULTS", "{}"))
                if not isinstance(results, dict):
                    raise ValueError("unexpected result shape")
            except ValueError:
                results = {}
                warnings.append("Workflow lane results malformed; outcomes unverified.")
            report = final_report(data, results, checks, jobs)
            report["warnings"] = warnings
    except Exception as exc:
        report = {"advisory": True, "warnings": [f"Report incomplete: {type(exc).__name__}: {exc}"], "files": []}
    try:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
        rendered = markdown(report)
        args.output.with_suffix(".md").write_text(rendered)
        print(rendered)
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as summary:
                summary.write(rendered)
    except (OSError, ValueError, TypeError) as exc:
        print(f"Advisory report output unavailable: {type(exc).__name__}: {exc}", file=sys.stderr)
    return 0  # Findings are advisory; existing build/test gates remain authoritative.


if __name__ == "__main__":
    sys.exit(main())
