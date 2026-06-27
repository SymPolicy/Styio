#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import json
import re
import subprocess
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Sequence

from docs_config import collection_dirs

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
COLLECTION_DIRS = collection_dirs(ROOT)
PURPOSE_RE = re.compile(r"^(?:\*\*Purpose:\*\*|\[EN\] Purpose:)\s+.+$", re.M)
LAST_UPDATED_RE = re.compile(r"^(?:\*\*Last updated:\*\*|\[EN\] Last updated:)\s+[0-9]{4}-[0-9]{2}-[0-9]{2}\s*$", re.M)
LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
DATE_FILE_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}\.md$")
ADR_FILE_RE = re.compile(r"^ADR-[0-9]{4}-[a-z0-9-]+\.md$")
APPROVED_ADR_MARKDOWN = {"README.md", "INDEX.md", "IMPLEMENTED-DECISIONS.md"}
BENCHMARK_REPORT_SUMMARY_RE = re.compile(r"^benchmark/reports/[^/]+/summary\.md$")
APPROVED_TEST_DOC_NAMES = {"README.md", "REGRESSION-TEMPLATE.md"}
APPROVED_ROOT_MARKDOWN = {
    "CHANGELOG.md",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "DEPENDENCY-USAGE.md",
    "LICENSE-POLICY.md",
    "README.md",
    "README_zh.md",
    "RELEASE-POLICY.md",
    "SECURITY.md",
    "SUPPORT.md",
}
APPROVED_GITHUB_MARKDOWN = {
    ".github/PULL_REQUEST_TEMPLATE.md",
}
APPROVED_EXAMPLE_MARKDOWN = {
    "example/README.md",
}
PLAN_DOC_EXEMPT_NAMES = {"README.md", "INDEX.md"}
PLAN_REQUIRED_SECTIONS = ("前置条件", "验收条件")
PLAN_PRECONDITION_NEEDLES = ("并行", "子智能体", "基座")
FOUNDATION_KEY_RE = re.compile(r"^\*\*Foundation key:\*\*\s+([a-z0-9][a-z0-9-]*)\s*$", re.M)
VERSION_NAMING_TOKEN_RE = re.compile(
    r"(^|[-_.\s])(v[0-9]+|version[0-9]*|ver[0-9]+|new|old|legacy|latest)(?=$|[-_.\s])",
    re.I,
)
VERSION_NAMING_EXEMPT_NAMES = {"README", "README_zh", "INDEX", "IMPLEMENTED-DECISIONS"}
DOC_REUSE_NEEDLES = {
    "scripts/docs-scaffold.py": (
        "--reuse-reviewed",
        "existing docs/ADR/SSOT",
        "single-purpose document",
    ),
    "docs/README.md": (
        "Before creating a Markdown file",
        "update an existing owner",
    ),
    "docs/specs/DOCUMENTATION-POLICY.md": (
        "--reuse-reviewed",
        "single responsibility",
        "version-style",
        "Git history",
    ),
    "docs/adr/README.md": (
        "Existing ADR Search",
        "current decision only",
        "Git history",
    ),
    "workflows/ADD-REPO-FILE.md": (
        "Search existing docs",
        "single responsibility",
        "Existing owner files searched",
    ),
    "workflows/DOCS-MAINTENANCE-WORKFLOW.md": (
        "--reuse-reviewed",
        "existing owner",
        "current decision only",
    ),
    "workflows/DOCS-GATE.md": (
        "search existing owner documents and ADRs first",
        "--reuse-reviewed",
    ),
    "workflows/skills/styio-file-onboarding/skill.toml": (
        "existing owner",
        "one ADR per small implementation change",
        "old and new decisions side by side",
        "version-style",
    ),
}
VERSION_NAMING_NEEDLES = {
    "workflows/FEATURE-CUTOVER-WORKFLOW.md": (
        "version-style names",
        "feature or transformation result",
    ),
    "workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md": (
        "version-style names",
        "feature or transformation result",
    ),
    "workflows/skills/styio-feature-cutover/skill.toml": (
        "version-style",
        "feature or transformation result",
    ),
    "workflows/skills/styio-functional-commit-readiness/skill.toml": (
        "version-style",
        "feature or transformation result",
    ),
    "workflows/skills/styio-file-onboarding/references/file-onboarding-surface.md": (
        "version-style",
        "feature or transformation result",
    ),
    "scripts/docs-scaffold.py": (
        "VERSION_NAMING_TOKEN_RE",
        "version-style document names are not allowed",
    ),
}
LOCAL_INFO_POLICY_NEEDLES = {
    "scripts/local-info-leak-gate.py": (
        "WINDOWS_ABSOLUTE_PATH_RE",
        "WARNING",
        "developer-machine or server-specific",
    ),
    "scripts/workflow-scheduler.py": (
        "local-info-leak-worktree",
        "local-info-leak-push",
    ),
    "workflows/LOCAL-INFO-LEAK-GATE.md": (
        "developer-machine",
        "<workspace-root>",
        "WARNING",
    ),
    "workflows/LOCAL-INFO-LEAK-GATE.toml": (
        "local-info-leak-gate",
        "<workspace-root>",
    ),
    "workflows/WORKFLOW-ORCHESTRATION.md": (
        "LOCAL-INFO-LEAK-GATE.md",
        "local-info scan",
    ),
    "workflows/README.md": (
        "LOCAL-INFO-LEAK-GATE.md",
        "developer-machine and server-specific",
    ),
    "workflows/TOOL-SKILL-REGISTRY-GATE.toml": (
        "local-info-leak-gate",
        "scripts/local-info-leak-gate.py",
    ),
    "workflows/TOOL-SKILL-REGISTRY-GATE.md": (
        "local info leak gate",
        "placeholders",
    ),
    "workflows/skills/README.md": (
        "developer-machine paths",
        "<workspace-root>",
    ),
    "workflows/ADD-REPO-FILE.md": (
        "local-info-leak-gate.py",
        "developer-machine paths",
    ),
    "workflows/DOCS-MAINTENANCE-WORKFLOW.md": (
        "local-info-leak-gate.py",
        "server-machine",
    ),
    "docs/specs/DOCUMENTATION-POLICY.md": (
        "local-info-leak-gate.py",
        "<workspace-root>",
    ),
}
ADR_REQUIRED_SECTIONS = ("Status", "Context", "Decision", "Alternatives", "Consequences")
ADR_FORBIDDEN_HISTORY_HEADINGS = {
    "History",
    "Decision History",
    "Previous Decision",
    "Previous Decisions",
    "Old Decision",
    "Old Decisions",
    "Superseded",
    "Changelog",
    "Change Log",
    "Migration History",
}
COMMIT_READINESS_NEEDLES = {
    "workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md": (
        "upstream",
        "downstream",
        "targeted feature",
        "objective blocker",
    ),
    "workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.toml": (
        "functional-commit-readiness",
        "upstream/downstream",
        "unrecorded_unable_to_verify",
    ),
    "scripts/delivery-gate.sh": (
        "FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md",
        "STYIO_COMMIT_READINESS_REQUIRE",
        "STYIO_COMMIT_READINESS_CONFIRMED",
        "version-style naming",
    ),
    "scripts/checkpoint-health.sh": (
        "FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md",
        "upstream/downstream",
        "version-style naming",
    ),
    "scripts/install-repo-hygiene-hooks.sh": (
        "STYIO_COMMIT_READINESS_REQUIRE=1",
        "delivery-gate.sh",
    ),
    ".github/PULL_REQUEST_TEMPLATE.md": (
        "Functional changes:",
        "upstream/downstream",
        "version-style names",
        "unable-to-verify blockers",
    ),
    "workflows/README.md": (
        "FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md",
        "before commit",
    ),
    "workflows/DELIVERY-GATE.md": (
        "FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md",
        "upstream/downstream",
        "version-style naming",
    ),
    "workflows/CHECKPOINT-HEALTH.md": (
        "FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md",
        "upstream/downstream",
        "version-style naming",
    ),
    "workflows/REPO-HYGIENE-COMMIT-STANDARD.md": (
        "FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md",
        "STYIO_COMMIT_READINESS_CONFIRMED",
        "version-style naming check",
    ),
}
PARAM_RESOURCE_PSEUDO_DEF_RE = re.compile(r"@[A-Za-z_][A-Za-z0-9_]*\s*[\{\(][^\n`]*\s*:=")
FILE_PATH_PSEUDO_PRIMITIVE_RE = re.compile(r"\bfile\s*\(\s*path\s*\)")
RESOURCE_PSEUDO_DEF_NEGATION_RE = re.compile(r"\b(do not|don't|must not|never|not|invalid|forbid|forbidden|reject|rejected)\b", re.I)
PUBLIC_WORDING_FORBIDDEN_PATTERNS = (
    (re.compile(r"\bfastest\b", re.I), "absolute speed superlative"),
    (re.compile(r"\bbest\s+practices\b", re.I), "absolute practice superlative"),
    (re.compile(r"\bbest[- ]in[- ]class\b", re.I), "unsupported superiority wording"),
    (re.compile(r"\bworld[- ]class\b", re.I), "unsupported superiority wording"),
    (re.compile(r"\bindustry[- ]leading\b", re.I), "unsupported superiority wording"),
    (re.compile(r"\bstrongest\b", re.I), "unsupported superiority wording"),
    (re.compile(r"\boptimal\b", re.I), "unsupported superiority wording"),
    (re.compile(r"\bgenuinely\s+novel\b", re.I), "unsupported novelty wording"),
    (re.compile(r"\bfully\s+functional\b", re.I), "over-broad maturity wording"),
    (re.compile(r"\bclaim(?:s|ed|ing)?\b", re.I), "public-claim wording"),
    (re.compile(r"\bperformance\s+claims\b", re.I), "unsupported public-claim wording"),
    (re.compile(r"\bbenchmark(?:ed|ing)\s+against\b", re.I), "external-comparison wording without evidence scope"),
    (re.compile(r"\bR" r"ust[- ]equivalent\b|\bR" r"ust\s+equivalence\b|R" r"ust\s*等价", re.I), "unsupported language-equivalence wording"),
    (re.compile(r"宣称|声称"), "public-claim wording"),
    (re.compile(r"对标|最快|最佳|最好|最强|领先|世界级|一流"), "unsupported public-positioning wording"),
)


@dataclass(frozen=True)
class ManifestEntry:
    path: Path
    status: str
    reason: str
    character_count: int
    word_count: int

    @property
    def rel_path(self) -> str:
        return self.path.as_posix()


def strip_code_fences(text: str) -> str:
    return re.sub(r"```.*?```", "", text, flags=re.S)


def looks_like_repo_link(target: str) -> bool:
    if target.startswith("./") or target.startswith("../") or target.endswith("/"):
        return True
    return Path(target).suffix in {
        ".md",
        ".txt",
        ".out",
        ".err",
        ".json",
        ".jsonl",
        ".toml",
        ".yml",
        ".yaml",
        ".sh",
        ".py",
        ".hpp",
        ".h",
        ".cpp",
        ".c",
        ".ll",
        ".styio",
        ".png",
        ".jpg",
        ".jpeg",
        ".svg",
    }


def has_prefix(path: Path, prefix: Path) -> bool:
    prefix_parts = prefix.parts
    return path.parts[: len(prefix_parts)] == prefix_parts


def is_archive_doc(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    return has_prefix(rel, Path("docs/archive"))


def skip_link_check(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    if not is_archive_doc(path):
        return False
    if rel.name in {"INDEX.md", "ARCHIVE-LEDGER.md"}:
        return False
    if rel.parent == Path("docs/archive"):
        return False
    if rel.parent in {Path("docs/archive/history"), Path("docs/archive/review")} and rel.name == "README.md":
        return False
    return True


def iter_docs() -> Iterable[Path]:
    return sorted(DOCS.rglob("*.md"))


def git_tracked_markdown_paths() -> List[Path]:
    proc = subprocess.run(
        ["git", "ls-files", "-z", "--", "*.md"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        raise RuntimeError(f"git ls-files failed: {detail}")
    paths: List[Path] = []
    for raw in proc.stdout.split("\0"):
        if not raw:
            continue
        rel = Path(raw)
        if (ROOT / rel).exists():
            paths.append(rel)
    return sorted(paths, key=lambda path: path.as_posix())


def git_worktree_markdown_paths() -> List[Path]:
    proc = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", "*.md"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        raise RuntimeError(f"git ls-files worktree scan failed: {detail}")
    paths: List[Path] = []
    for raw in proc.stdout.split("\0"):
        if not raw:
            continue
        rel = Path(raw)
        if (ROOT / rel).exists():
            paths.append(rel)
    return sorted({path.as_posix(): path for path in paths}.values(), key=lambda path: path.as_posix())


def filesystem_markdown_paths() -> List[Path]:
    paths: List[Path] = []
    for path in ROOT.rglob("*.md"):
        rel = path.relative_to(ROOT)
        if ".git" in rel.parts:
            continue
        paths.append(rel)
    return sorted(paths, key=lambda candidate: candidate.as_posix())


def classify_markdown(path: Path) -> ManifestEntry:
    rel = path.as_posix()
    character_count, word_count = measure_markdown(path)

    if rel in APPROVED_ROOT_MARKDOWN:
        return ManifestEntry(path, "valid", "approved repository root community document", character_count, word_count)
    if rel in APPROVED_GITHUB_MARKDOWN:
        return ManifestEntry(path, "valid", "approved GitHub community template", character_count, word_count)
    if rel in APPROVED_EXAMPLE_MARKDOWN:
        return ManifestEntry(path, "valid", "approved runnable example documentation", character_count, word_count)
    if BENCHMARK_REPORT_SUMMARY_RE.match(rel):
        return ManifestEntry(
            path,
            "invalid",
            "generated benchmark report summary; promote conclusions into tracked docs instead",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("docs")):
        return ManifestEntry(path, "valid", "approved docs/ collection", character_count, word_count)
    if has_prefix(path, Path("benchmark")):
        return ManifestEntry(path, "valid", "approved benchmark documentation", character_count, word_count)
    if has_prefix(path, Path("library")):
        return ManifestEntry(path, "valid", "approved standard-library documentation", character_count, word_count)
    if has_prefix(path, Path("templates")):
        return ManifestEntry(path, "valid", "approved reusable template documentation", character_count, word_count)
    if has_prefix(path, Path("workflows")):
        return ManifestEntry(path, "valid", "approved root workflow documentation and skills", character_count, word_count)
    if has_prefix(path, Path("src/StyioServices")) and path.name in {"README.md", "MANIFEST.md"}:
        return ManifestEntry(
            path,
            "valid",
            "approved source-level service README or manifest",
            character_count,
            word_count,
        )
    if rel == "grammar/tree-sitter-styio/README.md":
        return ManifestEntry(
            path,
            "valid",
            "approved repository-local grammar documentation",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("tests")):
        if path.name in APPROVED_TEST_DOC_NAMES:
            return ManifestEntry(path, "valid", "approved test workflow documentation", character_count, word_count)
        return ManifestEntry(
            path,
            "invalid",
            "markdown under tests/ must be a README.md or approved template",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("src")):
        return ManifestEntry(
            path,
            "invalid",
            "markdown under src/ is outside approved documentation locations",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("grammar")):
        return ManifestEntry(
            path,
            "invalid",
            "markdown under grammar/ is only approved for grammar/tree-sitter-styio/README.md",
            character_count,
            word_count,
        )
    if has_prefix(path, Path("build")) or path.parts[0].startswith("build-"):
        return ManifestEntry(
            path,
            "invalid",
            "build output or vendored dependency markdown is not a project document",
            character_count,
            word_count,
        )
    return ManifestEntry(
        path,
        "invalid",
        "markdown path is outside every approved repository documentation location",
        character_count,
        word_count,
    )


def is_han_character(ch: str) -> bool:
    code = ord(ch)
    return (
        0x3400 <= code <= 0x4DBF
        or 0x4E00 <= code <= 0x9FFF
        or 0xF900 <= code <= 0xFAFF
        or 0x20000 <= code <= 0x2A6DF
        or 0x2A700 <= code <= 0x2B73F
        or 0x2B740 <= code <= 0x2B81F
        or 0x2B820 <= code <= 0x2CEAF
        or 0x2CEB0 <= code <= 0x2EBEF
        or 0x30000 <= code <= 0x3134F
        or 0x2F800 <= code <= 0x2FA1F
    )


def is_ascii_word_char(ch: str) -> bool:
    return ch.isascii() and (ch.isalnum() or ch == "_")


def count_words(text: str) -> int:
    total = 0
    index = 0
    while index < len(text):
        ch = text[index]
        if ch.isspace():
            index += 1
            continue
        if is_han_character(ch):
            total += 1
            index += 1
            continue
        if is_ascii_word_char(ch):
            total += 1
            index += 1
            while index < len(text) and is_ascii_word_char(text[index]):
                index += 1
            continue
        total += 1
        index += 1
    return total


def measure_markdown(path: Path) -> tuple[int, int]:
    text = (ROOT / path).read_text(encoding="utf-8")
    return len(text), count_words(text)


def collect_manifest(source: str) -> List[ManifestEntry]:
    if source == "worktree":
        paths = git_worktree_markdown_paths()
    elif source == "git":
        paths = git_tracked_markdown_paths()
    elif source == "filesystem":
        paths = filesystem_markdown_paths()
    else:
        raise ValueError(f"unknown manifest source: {source}")
    return [classify_markdown(path) for path in paths]


def filter_manifest(entries: Sequence[ManifestEntry], status: str) -> List[ManifestEntry]:
    if status == "all":
        return list(entries)
    return [entry for entry in entries if entry.status == status]


def make_tree(entries: Sequence[ManifestEntry]) -> Dict[str, object]:
    root: Dict[str, object] = {
        "name": ".",
        "type": "directory",
        "path": ".",
        "children": {},
    }

    for entry in entries:
        node = root
        segments = entry.path.parts
        prefix: List[str] = []
        for segment in segments[:-1]:
            prefix.append(segment)
            children = node["children"]  # type: ignore[index]
            child = children.get(segment)  # type: ignore[union-attr]
            if child is None:
                child = {
                    "name": segment,
                    "type": "directory",
                    "path": "/".join(prefix),
                    "children": {},
                }
                children[segment] = child  # type: ignore[index]
            node = child
        children = node["children"]  # type: ignore[index]
        children[segments[-1]] = {
            "name": segments[-1],
            "type": "file",
            "path": entry.rel_path,
            "status": entry.status,
            "reason": entry.reason,
        }

    return freeze_tree(root)


def freeze_tree(node: Dict[str, object]) -> Dict[str, object]:
    if node["type"] != "directory":
        return dict(node)

    raw_children = list(node["children"].values())  # type: ignore[index]
    frozen_children = [freeze_tree(child) for child in raw_children]
    frozen_children.sort(key=lambda child: (child["type"] != "directory", str(child["name"]).lower()))

    return {
        "name": node["name"],
        "type": node["type"],
        "path": node["path"],
        "children": frozen_children,
    }


def render_tree(tree: Dict[str, object], annotate_reason: bool) -> str:
    lines = ["."]

    def visit(children: Sequence[Dict[str, object]], prefix: str) -> None:
        for index, child in enumerate(children):
            is_last = index == len(children) - 1
            branch = "└── " if is_last else "├── "
            label = str(child["name"])
            if child["type"] == "directory":
                label = f"{label}/"
            elif annotate_reason:
                label = f"{label}  [{child['reason']}]"
            lines.append(f"{prefix}{branch}{label}")
            if child["type"] == "directory":
                extension = "    " if is_last else "│   "
                visit(child["children"], prefix + extension)  # type: ignore[arg-type]

    visit(tree["children"], "")  # type: ignore[arg-type]
    return "\n".join(lines)


def render_list(entries: Sequence[ManifestEntry], include_reason: bool) -> str:
    if not entries:
        return "(empty)"
    lines: List[str] = []
    for entry in entries:
        if include_reason:
            lines.append(f"- {entry.rel_path}: {entry.reason}")
        else:
            lines.append(f"- {entry.rel_path}")
    return "\n".join(lines)


def aggregate_text_stats(entries: Sequence[ManifestEntry]) -> Dict[str, int]:
    return {
        "characters": sum(entry.character_count for entry in entries),
        "words": sum(entry.word_count for entry in entries),
    }


def manifest_payload(entries: Sequence[ManifestEntry], filtered: Sequence[ManifestEntry], source: str, status: str) -> Dict[str, object]:
    counts = {
        "total": len(entries),
        "valid": sum(1 for entry in entries if entry.status == "valid"),
        "invalid": sum(1 for entry in entries if entry.status == "invalid"),
        "selected": len(filtered),
    }
    valid_entries = [entry for entry in entries if entry.status == "valid"]
    invalid_entries = [entry for entry in entries if entry.status == "invalid"]
    return {
        "root": ROOT.as_posix(),
        "source": source,
        "status_filter": status,
        "counts": counts,
        "text_stats": {
            "total": aggregate_text_stats(entries),
            "valid": aggregate_text_stats(valid_entries),
            "invalid": aggregate_text_stats(invalid_entries),
            "selected": aggregate_text_stats(filtered),
        },
        "entries": [
            {
                "path": entry.rel_path,
                "status": entry.status,
                "reason": entry.reason,
                "character_count": entry.character_count,
                "word_count": entry.word_count,
            }
            for entry in filtered
        ],
        "tree": make_tree(filtered),
    }


def write_output(text: str, output: Path | None) -> None:
    if output is None:
        print(text)
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")


def run_manifest(status: str, fmt: str, source: str, output: Path | None) -> int:
    entries = collect_manifest(source)
    filtered = filter_manifest(entries, status)
    selected_text_stats = aggregate_text_stats(filtered)

    if fmt == "json":
        payload = manifest_payload(entries, filtered, source, status)
        rendered = json.dumps(payload, ensure_ascii=False, indent=2)
    elif fmt == "tree":
        counts = {
            "total": len(entries),
            "valid": sum(1 for entry in entries if entry.status == "valid"),
            "invalid": sum(1 for entry in entries if entry.status == "invalid"),
            "selected": len(filtered),
        }
        header = [
            f"source: {source}",
            f"status: {status}",
            f"selected: {counts['selected']} / total: {counts['total']} (valid={counts['valid']}, invalid={counts['invalid']})",
            f"selected_text: characters={selected_text_stats['characters']}, words={selected_text_stats['words']}",
            "",
        ]
        rendered = "\n".join(header) + render_tree(make_tree(filtered), annotate_reason=status != "valid")
    elif fmt == "list":
        counts = {
            "total": len(entries),
            "valid": sum(1 for entry in entries if entry.status == "valid"),
            "invalid": sum(1 for entry in entries if entry.status == "invalid"),
            "selected": len(filtered),
        }
        header = [
            f"source: {source}",
            f"status: {status}",
            f"selected: {counts['selected']} / total: {counts['total']} (valid={counts['valid']}, invalid={counts['invalid']})",
            f"selected_text: characters={selected_text_stats['characters']}, words={selected_text_stats['words']}",
            "",
        ]
        rendered = "\n".join(header) + render_list(filtered, include_reason=status != "valid")
    else:
        raise ValueError(f"unsupported format: {fmt}")

    write_output(rendered, output)
    return 0


def check_collection_dirs(errors: List[str]) -> None:
    for directory in COLLECTION_DIRS:
        readme = directory / "README.md"
        index = directory / "INDEX.md"
        if not readme.exists():
            errors.append(f"missing required README.md: {readme.relative_to(ROOT)}")
        if not index.exists():
            errors.append(f"missing required INDEX.md: {index.relative_to(ROOT)}")
        if readme.exists():
            head = readme.read_text(encoding="utf-8")[:2000]
            if "INDEX.md" not in head:
                errors.append(f"README does not point to INDEX.md: {readme.relative_to(ROOT)}")


def check_naming(errors: List[str]) -> None:
    for path in (DOCS / "history").glob("*.md"):
        if path.name not in {"README.md", "INDEX.md"} and not DATE_FILE_RE.match(path.name):
            errors.append(f"invalid history filename: {path.relative_to(ROOT)}")
    for path in (DOCS / "adr").glob("*.md"):
        if path.name not in APPROVED_ADR_MARKDOWN and not ADR_FILE_RE.match(path.name):
            errors.append(f"invalid ADR filename: {path.relative_to(ROOT)}")

def check_metadata(errors: List[str]) -> None:
    for path in iter_docs():
        text = path.read_text(encoding="utf-8")
        head = "\n".join(text.splitlines()[:16])
        if not PURPOSE_RE.search(head):
            errors.append(f"missing top-level Purpose metadata: {path.relative_to(ROOT)}")
        if not LAST_UPDATED_RE.search(head):
            errors.append(f"missing top-level Last updated metadata: {path.relative_to(ROOT)}")


def check_links(errors: List[str]) -> None:
    for path in iter_docs():
        if skip_link_check(path):
            continue
        text = strip_code_fences(path.read_text(encoding="utf-8"))
        for raw_target in LINK_RE.findall(text):
            target = raw_target.strip()
            if not target or target.startswith("http://") or target.startswith("https://") or target.startswith("mailto:") or target.startswith("#"):
                continue
            target = target.split("#", 1)[0]
            if not target:
                continue
            if re.search(r"\s", target):
                continue
            if not looks_like_repo_link(target):
                continue
            resolved = (path.parent / target).resolve()
            if not resolved.exists():
                errors.append(f"broken relative link in {path.relative_to(ROOT)} -> {raw_target}")


def check_generated_indexes(errors: List[str]) -> None:
    proc = subprocess.run(
        [sys.executable, str(ROOT / "scripts/docs-index.py"), "--check"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        errors.append(f"generated indexes are stale: {detail}")


def check_lifecycle(errors: List[str]) -> None:
    proc = subprocess.run(
        [sys.executable, str(ROOT / "scripts/docs-lifecycle.py"), "validate"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        errors.append(f"docs lifecycle validation failed: {detail}")


def check_team_docs_gate(errors: List[str]) -> None:
    if os.environ.get("STYIO_SKIP_TEAM_DOC_GATE") == "1":
        return
    proc = subprocess.run(
        [sys.executable, str(ROOT / "scripts/team-docs-gate.py")],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        errors.append(f"team docs gate failed: {detail}")


def check_workflow_toml(errors: List[str]) -> None:
    workflows = ROOT / "workflows"
    if not workflows.exists():
        return

    registry = workflows / "workflows.toml"
    if not registry.exists():
        errors.append("missing root workflow registry: workflows/workflows.toml")

    for doc in sorted(workflows.glob("*.md")):
        if doc.name in {"README.md", "INDEX.md"}:
            continue
        workflow_toml = doc.with_suffix(".toml")
        if not workflow_toml.exists():
            errors.append(f"missing TOML workflow definition for {doc.relative_to(ROOT)}")

    for path in workflows.rglob("*.toml"):
        try:
            tomllib.loads(path.read_text(encoding="utf-8"))
        except tomllib.TOMLDecodeError as exc:
            errors.append(f"invalid TOML in {path.relative_to(ROOT)}: {exc}")

    for path in workflows.rglob("*.yaml"):
        errors.append(f"YAML is not allowed for workflow/skill metadata: {path.relative_to(ROOT)}")
    for path in workflows.rglob("*.yml"):
        errors.append(f"YAML is not allowed for workflow/skill metadata: {path.relative_to(ROOT)}")

    skills_dir = workflows / "skills"
    if skills_dir.exists():
        for skill_dir in sorted(child for child in skills_dir.iterdir() if child.is_dir()):
            if not (skill_dir / "skill.toml").exists():
                errors.append(f"missing skill.toml: {skill_dir.relative_to(ROOT)}")


def h2_headings(text: str) -> List[str]:
    return [line[3:].strip() for line in text.splitlines() if line.startswith("## ")]


def section_body(text: str, heading: str) -> str:
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if line == f"## {heading}":
            start = index + 1
            break
    if start is None:
        return ""
    end = len(lines)
    for index in range(start, len(lines)):
        if lines[index].startswith("## "):
            end = index
            break
    return "\n".join(lines[start:end]).strip()


def check_plan_docs(errors: List[str]) -> None:
    plans_dir = DOCS / "plans"
    if not plans_dir.exists():
        return

    readme = plans_dir / "README.md"
    readme_text = readme.read_text(encoding="utf-8") if readme.exists() else ""
    if "## 通用基座计划索引" not in readme_text:
        errors.append("docs/plans/README.md is missing required section: ## 通用基座计划索引")

    foundation_keys: Dict[str, Path] = {}
    for path in sorted(plans_dir.glob("*.md")):
        if path.name in PLAN_DOC_EXEMPT_NAMES:
            continue
        rel = path.relative_to(ROOT)
        text = path.read_text(encoding="utf-8")
        headings = h2_headings(text)

        for heading in PLAN_REQUIRED_SECTIONS:
            if heading not in headings:
                errors.append(f"plan document is missing required section ## {heading}: {rel}")
        if all(heading in headings for heading in PLAN_REQUIRED_SECTIONS):
            if headings.index("前置条件") > headings.index("验收条件"):
                errors.append(f"plan document must place ## 前置条件 before ## 验收条件: {rel}")

        preconditions = section_body(text, "前置条件")
        if preconditions:
            for needle in PLAN_PRECONDITION_NEEDLES:
                if needle not in preconditions:
                    errors.append(f"plan ## 前置条件 must state {needle} policy: {rel}")
        acceptance = section_body(text, "验收条件")
        if acceptance and len(strip_code_fences(acceptance).strip()) < 20:
            errors.append(f"plan ## 验收条件 is too thin to be actionable: {rel}")

        keys = FOUNDATION_KEY_RE.findall(text)
        if len(keys) > 1:
            errors.append(f"plan document declares multiple foundation keys: {rel}")
        if keys:
            key = keys[0]
            previous = foundation_keys.get(key)
            if previous is not None:
                errors.append(
                    "duplicate foundation plan key "
                    f"{key}: {previous.relative_to(ROOT)} and {rel}"
                )
            foundation_keys[key] = path

    for key, path in sorted(foundation_keys.items()):
        if key not in readme_text or path.name not in readme_text:
            errors.append(
                "docs/plans/README.md foundation index must list "
                f"{key} -> {path.relative_to(ROOT)}"
            )


def check_commit_readiness_workflow(errors: List[str]) -> None:
    for rel_path, needles in COMMIT_READINESS_NEEDLES.items():
        path = ROOT / rel_path
        if not path.is_file():
            errors.append(f"commit-readiness workflow required file is missing: {rel_path}")
            continue
        text = path.read_text(encoding="utf-8")
        for needle in needles:
            if needle not in text:
                errors.append(f"commit-readiness workflow is not wired into {rel_path}: missing {needle!r}")


def check_doc_reuse_policy(errors: List[str]) -> None:
    for rel_path, needles in DOC_REUSE_NEEDLES.items():
        path = ROOT / rel_path
        if not path.is_file():
            errors.append(f"document-reuse policy required file is missing: {rel_path}")
            continue
        text = path.read_text(encoding="utf-8")
        for needle in needles:
            if needle not in text:
                errors.append(f"document-reuse policy is not wired into {rel_path}: missing {needle!r}")


def check_adr_docs(errors: List[str]) -> None:
    adr_dir = DOCS / "adr"
    if not adr_dir.exists():
        return
    for path in sorted(adr_dir.glob("ADR-*.md")):
        rel = path.relative_to(ROOT)
        if not ADR_FILE_RE.match(path.name):
            continue
        text = path.read_text(encoding="utf-8")
        headings = h2_headings(text)
        for heading in ADR_REQUIRED_SECTIONS:
            if heading not in headings:
                errors.append(f"active ADR is missing required section ## {heading}: {rel}")
        for heading in headings:
            if heading in ADR_FORBIDDEN_HISTORY_HEADINGS:
                errors.append(
                    "active ADR must record the current decision only; "
                    f"use Git history for old decision text: {rel} has ## {heading}"
                )


def version_naming_part(path: Path) -> str:
    for part in path.parts:
        stem = Path(part).stem
        if stem in VERSION_NAMING_EXEMPT_NAMES:
            continue
        if DATE_FILE_RE.match(f"{stem}.md"):
            continue
        if ADR_FILE_RE.match(f"{stem}.md"):
            continue
        if VERSION_NAMING_TOKEN_RE.search(stem):
            return part
    return ""


def check_version_naming_policy(errors: List[str]) -> None:
    for rel_path, needles in VERSION_NAMING_NEEDLES.items():
        path = ROOT / rel_path
        if not path.is_file():
            errors.append(f"version-naming policy required file is missing: {rel_path}")
            continue
        text = path.read_text(encoding="utf-8")
        for needle in needles:
            if needle not in text:
                errors.append(f"version-naming policy is not wired into {rel_path}: missing {needle!r}")

    for entry in collect_manifest("worktree"):
        if entry.status != "valid":
            continue
        rel = entry.path
        if rel.parts[0] not in {"docs", "workflows"}:
            continue
        bad_name = version_naming_part(rel)
        if bad_name:
            errors.append(
                "version-style names are not allowed for active docs/workflows; "
                f"name by feature or transformation result instead: {entry.rel_path} ({bad_name})"
            )

    for path in sorted((ROOT / "workflows").rglob("*.toml")):
        try:
            data = tomllib.loads(path.read_text(encoding="utf-8"))
        except tomllib.TOMLDecodeError:
            continue
        candidates: List[tuple[str, str]] = []
        for field in ("id", "title", "name", "display_name"):
            value = data.get(field)
            if isinstance(value, str):
                candidates.append((field, value))
        skill = data.get("skill")
        if isinstance(skill, dict):
            for field in ("name", "display_name"):
                value = skill.get(field)
                if isinstance(value, str):
                    candidates.append((f"skill.{field}", value))
        for field, value in candidates:
            if VERSION_NAMING_TOKEN_RE.search(value):
                errors.append(
                    "version-style workflow/skill names are not allowed; "
                    f"name by feature or transformation result instead: {path.relative_to(ROOT)} {field}={value!r}"
                )


def check_local_info_policy(errors: List[str]) -> None:
    for rel_path, needles in LOCAL_INFO_POLICY_NEEDLES.items():
        path = ROOT / rel_path
        if not path.is_file():
            errors.append(f"local-info leak policy required file is missing: {rel_path}")
            continue
        text = path.read_text(encoding="utf-8")
        for needle in needles:
            if needle not in text:
                errors.append(f"local-info leak policy is not wired into {rel_path}: missing {needle!r}")

    skill_dir = ROOT / "workflows" / "skills"
    for path in sorted(skill_dir.glob("*/skill.toml")):
        text = path.read_text(encoding="utf-8")
        if "developer-machine or server-machine paths" not in text:
            errors.append(
                "repo-local skills must forbid local/server machine details: "
                f"{path.relative_to(ROOT)}"
            )
        if "local/server info placeholder check" not in text:
            errors.append(
                "repo-local skills must report local/server placeholder checks: "
                f"{path.relative_to(ROOT)}"
            )


def check_resource_identifier_governance(errors: List[str]) -> None:
    search_roots = [DOCS, ROOT / "workflows"]
    suffixes = {".md", ".toml"}
    for search_root in search_roots:
        if not search_root.exists():
            continue
        for path in sorted(p for p in search_root.rglob("*") if p.is_file() and p.suffix in suffixes):
            if is_archive_doc(path):
                continue
            for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                if PARAM_RESOURCE_PSEUDO_DEF_RE.search(line) and not RESOURCE_PSEUDO_DEF_NEGATION_RE.search(line):
                    errors.append(
                        "parameterized resource expressions are not declaration heads: "
                        f"{path.relative_to(ROOT)}:{line_no}"
                    )
                if FILE_PATH_PSEUDO_PRIMITIVE_RE.search(line) and not RESOURCE_PSEUDO_DEF_NEGATION_RE.search(line):
                    errors.append(
                        "file(path) is not an allowed Styio resource primitive: "
                        f"{path.relative_to(ROOT)}:{line_no}"
                    )


def check_public_wording(errors: List[str]) -> None:
    for entry in collect_manifest("worktree"):
        if entry.status != "valid":
            continue
        path = ROOT / entry.path
        text = strip_code_fences(path.read_text(encoding="utf-8"))
        for line_no, line in enumerate(text.splitlines(), start=1):
            for pattern, reason in PUBLIC_WORDING_FORBIDDEN_PATTERNS:
                if pattern.search(line):
                    errors.append(
                        "public documentation wording is not evidence-scoped "
                        f"({reason}): {entry.rel_path}:{line_no}"
                    )


def run_audit() -> int:
    errors: List[str] = []
    check_collection_dirs(errors)
    check_naming(errors)
    check_metadata(errors)
    check_links(errors)
    check_generated_indexes(errors)
    check_lifecycle(errors)
    check_team_docs_gate(errors)
    check_workflow_toml(errors)
    check_plan_docs(errors)
    check_commit_readiness_workflow(errors)
    check_doc_reuse_policy(errors)
    check_adr_docs(errors)
    check_version_naming_policy(errors)
    check_local_info_policy(errors)
    check_resource_identifier_governance(errors)
    check_public_wording(errors)

    if errors:
        print("docs audit failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("docs audit passed")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit docs structure, metadata, links, and generated indexes, or print a repository-wide Markdown manifest."
    )
    parser.add_argument(
        "--manifest",
        choices=["valid", "invalid", "all"],
        help="Print a repository-wide Markdown manifest instead of running the docs audit.",
    )
    parser.add_argument(
        "--format",
        choices=["tree", "list", "json"],
        default="tree",
        help="Output format for --manifest. Defaults to tree.",
    )
    parser.add_argument(
        "--source",
        choices=["worktree", "git", "filesystem"],
        default="worktree",
        help="Manifest source. worktree scans tracked + unignored Markdown, git scans tracked Markdown only, and filesystem scans every Markdown file under the worktree.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Optional output file for --manifest. When omitted, output goes to stdout.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.manifest:
        return run_manifest(status=args.manifest, fmt=args.format, source=args.source, output=args.output)
    return run_audit()


if __name__ == "__main__":
    raise SystemExit(main())
