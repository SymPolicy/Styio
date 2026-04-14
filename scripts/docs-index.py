#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional

ROOT = Path(__file__).resolve().parents[1]
TODAY = "2026-04-14"
COLLECTION_DIRS = [
    Path("docs"),
    Path("docs/design"),
    Path("docs/specs"),
    Path("docs/review"),
    Path("docs/plans"),
    Path("docs/for-ide"),
    Path("docs/for_spio"),
    Path("docs/assets"),
    Path("docs/assets/workflow"),
    Path("docs/assets/templates"),
    Path("docs/adr"),
    Path("docs/history"),
    Path("docs/milestones"),
]

INDEX_META = {
    "docs": ("Docs Index", "Provide the generated inventory for `docs/`; directory boundaries and maintenance rules live in [README.md](./README.md)."),
    "docs/design": ("Design Index", "Provide the generated inventory for `docs/design/`; document boundaries and naming rules live in [README.md](./README.md)."),
    "docs/specs": ("Specs Index", "Provide the generated inventory for `docs/specs/`; document boundaries and naming rules live in [README.md](./README.md)."),
    "docs/review": ("Review Index", "Provide the generated inventory for `docs/review/`; document boundaries and naming rules live in [README.md](./README.md)."),
    "docs/plans": ("Plans Index", "Provide the generated inventory for `docs/plans/`; document boundaries and naming rules live in [README.md](./README.md)."),
    "docs/for-ide": ("For IDE Index", "Provide the generated inventory for `docs/for-ide/`; IDE embedding, LSP usage, and edit-time parser guidance live in [README.md](./README.md)."),
    "docs/for_spio": ("For Spio Index", "Provide the generated inventory for `docs/for_spio/`; handoff boundaries and coordination rules for `styio-spio` live in [README.md](./README.md)."),
    "docs/assets": ("Assets Index", "Provide the generated inventory for `docs/assets/`; asset boundaries and reuse rules live in [README.md](./README.md)."),
    "docs/assets/workflow": ("Workflow Assets Index", "Provide the generated inventory for `docs/assets/workflow/`; workflow boundaries and reuse rules live in [README.md](./README.md)."),
    "docs/assets/templates": ("Template Assets Index", "Provide the generated inventory for `docs/assets/templates/`; template boundaries and reuse rules live in [README.md](./README.md)."),
    "docs/adr": ("ADR Index", "Provide the generated inventory for `docs/adr/`; decision-record conventions live in [README.md](./README.md)."),
    "docs/history": ("History Index", "Provide the generated inventory for `docs/history/`; recovery rules live in [README.md](./README.md)."),
    "docs/milestones": ("Milestones Index", "Provide the generated inventory for `docs/milestones/`; freeze-batch rules live in [README.md](./README.md)."),
}

TITLE_RE = re.compile(r"^#\s+(.+?)\s*$", re.M)
PURPOSE_RE = re.compile(r"^\*\*Purpose:\*\*\s+(.+?)\s*$", re.M)
LAST_UPDATED_RE = re.compile(r"^\*\*Last updated:\*\*\s+([0-9]{4}-[0-9]{2}-[0-9]{2})\s*$", re.M)
LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")


@dataclass
class Entry:
    rel_path: str
    link_target: str
    label: str
    summary: str
    is_dir: bool
    last_updated: str


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def extract_title(path: Path) -> str:
    match = TITLE_RE.search(read_text(path))
    if not match:
        return path.stem
    return compact_plain(match.group(1))


def extract_purpose(path: Path) -> str:
    match = PURPOSE_RE.search(read_text(path))
    if not match:
        return f"Inventory entry for `{path.name}`."
    return compact_plain(match.group(1))


def extract_last_updated(path: Path) -> str:
    match = LAST_UPDATED_RE.search(read_text(path))
    if match:
        return match.group(1)
    return TODAY


def compact_plain(text: str) -> str:
    text = LINK_RE.sub(lambda m: m.group(1), text)
    for token in ("**", "`", "__"):
        text = text.replace(token, "")
    text = text.replace("|", "\\|")
    text = re.sub(r"\s+", " ", text).strip()
    return text


def rel_link(from_dir: Path, target: Path) -> str:
    rel = Path(os.path.relpath(target, from_dir))
    text = rel.as_posix()
    if not text.startswith("."):
        text = f"./{text}"
    return text


def choose_dir_entry(path: Path) -> Optional[Path]:
    for name in ("INDEX.md", "README.md", "00-Milestone-Index.md"):
        candidate = path / name
        if candidate.exists():
            return candidate
    return None


def choose_dir_summary_source(path: Path) -> Optional[Path]:
    for name in ("README.md", "INDEX.md", "00-Milestone-Index.md"):
        candidate = path / name
        if candidate.exists():
            return candidate
    return None


def child_sort_key(base: Path, path: Path):
    name = path.name
    if base.as_posix() in {"docs/history", "docs/milestones"}:
        return (0 if path.is_dir() else 1, -int(re.sub(r"[^0-9]", "", name) or 0), name)
    if base.as_posix() == "docs/adr":
        return (0 if path.is_dir() else 1, name)
    return (0 if path.is_dir() else 1, name.lower())


def build_entries(base: Path) -> List[Entry]:
    entries: List[Entry] = []
    children = [p for p in base.iterdir() if p.name not in {"README.md", "INDEX.md"} and not p.name.startswith(".")]
    for child in sorted(children, key=lambda p: child_sort_key(base, p)):
        if child.is_dir():
            entry_target = choose_dir_entry(child)
            summary_source = choose_dir_summary_source(child)
            if entry_target is None or summary_source is None:
                continue
            rel_path = f"{child.name}/"
            link_target = rel_link(base, entry_target)
            label = extract_title(entry_target)
            summary = extract_purpose(summary_source)
            last_updated = extract_last_updated(summary_source)
            entries.append(Entry(rel_path, link_target, label, summary, True, last_updated))
            continue
        if child.suffix != ".md":
            continue
        rel_path = child.name
        link_target = rel_link(base, child)
        label = extract_title(child)
        summary = extract_purpose(child)
        last_updated = extract_last_updated(child)
        entries.append(Entry(rel_path, link_target, label, summary, False, last_updated))
    return entries


def render_table(entries: Iterable[Entry]) -> List[str]:
    rows = ["| Path | Entry | Summary |", "|------|-------|---------|"]
    for entry in entries:
        path_text = f"`{entry.rel_path}`"
        label = entry.label.replace("|", "\\|")
        rows.append(f"| {path_text} | [{label}]({entry.link_target}) | {entry.summary} |")
    return rows


def render_index(base: Path) -> str:
    rel = base.relative_to(ROOT).as_posix()
    title, purpose = INDEX_META[rel]
    entries = build_entries(base)
    dir_entries = [e for e in entries if e.is_dir]
    file_entries = [e for e in entries if not e.is_dir]
    updated = max((e.last_updated for e in entries), default=TODAY)

    lines = [
        f"# {title}",
        "",
        f"**Purpose:** {purpose}",
        "",
        f"**Last updated:** {updated}",
        "",
        "> Generated by `python3 scripts/docs-index.py --write`. Edit `README.md` for scope and rules, then re-run the generator after docs-tree changes.",
    ]

    if dir_entries:
        lines.extend(["", "## Directories", ""])
        lines.extend(render_table(dir_entries))

    if file_entries:
        lines.extend(["", "## Files", ""])
        lines.extend(render_table(file_entries))

    lines.append("")
    return "\n".join(lines)


def write_indexes(check: bool) -> int:
    failures: List[str] = []
    for rel_dir in COLLECTION_DIRS:
        base = ROOT / rel_dir
        index_path = base / "INDEX.md"
        expected = render_index(base)
        current = index_path.read_text(encoding="utf-8") if index_path.exists() else None
        if check:
            if current != expected:
                failures.append(str(index_path.relative_to(ROOT)))
            continue
        index_path.write_text(expected, encoding="utf-8")
    if failures:
        print("Out-of-date generated indexes:", file=sys.stderr)
        for item in failures:
            print(f"  - {item}", file=sys.stderr)
        print("Run: python3 scripts/docs-index.py --write", file=sys.stderr)
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate or verify docs INDEX.md files.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true", help="Rewrite generated docs INDEX.md files")
    group.add_argument("--check", action="store_true", help="Verify generated docs INDEX.md files are up to date")
    args = parser.parse_args()
    return write_indexes(check=args.check)


if __name__ == "__main__":
    raise SystemExit(main())
