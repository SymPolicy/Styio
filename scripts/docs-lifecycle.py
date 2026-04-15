#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Dict, Iterable, List, Sequence

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
ARCHIVE_ROOT = DOCS / "archive"
ROLLUP_ROOT = DOCS / "rollups"
MANIFEST_PATH = ARCHIVE_ROOT / "ARCHIVE-MANIFEST.json"
LEDGER_PATH = ARCHIVE_ROOT / "ARCHIVE-LEDGER.md"
TODAY = date.today().isoformat()
DATE_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}$")
MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
FAMILY_ORDER = {
    "history": 0,
    "dated_review": 1,
    "superseded_milestone": 2,
}
ALLOWED_STATUSES = {"active", "summarized_active", "pending_archive", "archived"}
ACTIVE_FAMILIES = {"history", "dated_review"}


@dataclass(frozen=True)
class LifecycleEntry:
    source_path: str
    family: str
    status: str
    archive_path: str
    theme: str | None = None
    value: str | None = None
    targets: tuple[str, ...] = ()
    archived_at: str | None = None

    @property
    def source(self) -> Path:
        return Path(self.source_path)

    @property
    def archive(self) -> Path:
        return Path(self.archive_path)

    def to_dict(self) -> Dict[str, object]:
        data: Dict[str, object] = {
            "source_path": self.source_path,
            "family": self.family,
            "status": self.status,
            "archive_path": self.archive_path,
        }
        if self.theme is not None:
            data["theme"] = self.theme
        if self.value is not None:
            data["value"] = self.value
        if self.targets:
            data["targets"] = list(self.targets)
        if self.archived_at is not None:
            data["archived_at"] = self.archived_at
        return data

    @classmethod
    def from_dict(cls, data: Dict[str, object]) -> "LifecycleEntry":
        return cls(
            source_path=str(data["source_path"]),
            family=str(data["family"]),
            status=str(data["status"]),
            archive_path=str(data["archive_path"]),
            theme=str(data["theme"]) if data.get("theme") is not None else None,
            value=str(data["value"]) if data.get("value") is not None else None,
            targets=tuple(str(item) for item in data.get("targets", [])),
            archived_at=str(data["archived_at"]) if data.get("archived_at") is not None else None,
        )


def default_manifest() -> Dict[str, object]:
    return {
        "version": 1,
        "last_updated": TODAY,
        "archive_root": "docs/archive",
        "rollup_root": "docs/rollups",
        "keep_window": {
            "history": 1,
            "dated_review": 1,
            "superseded_milestone": 0,
        },
        "entries": [],
    }


def normalize_repo_path(text: str) -> Path:
    raw = Path(text)
    if raw.is_absolute():
        resolved = raw.resolve()
        try:
            return resolved.relative_to(ROOT)
        except ValueError as exc:
            raise ValueError(f"path is outside repository root: {text}") from exc
    return raw


def repo_abspath(path: Path) -> Path:
    return (ROOT / path).resolve()


def ensure_manifest_exists() -> Dict[str, object]:
    if MANIFEST_PATH.exists():
        return load_manifest()
    manifest = default_manifest()
    save_manifest(manifest)
    write_ledger(manifest)
    return manifest


def load_manifest() -> Dict[str, object]:
    if not MANIFEST_PATH.exists():
        raise RuntimeError(f"missing lifecycle manifest: {MANIFEST_PATH.relative_to(ROOT)}")
    data = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise RuntimeError("archive manifest must be a JSON object")
    return data


def save_manifest(manifest: Dict[str, object]) -> None:
    manifest["entries"] = [entry.to_dict() for entry in sort_entries(entries_from_manifest(manifest))]
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def entries_from_manifest(manifest: Dict[str, object]) -> List[LifecycleEntry]:
    raw_entries = manifest.get("entries", [])
    if not isinstance(raw_entries, list):
        raise RuntimeError("archive manifest entries must be a JSON array")
    return [LifecycleEntry.from_dict(item) for item in raw_entries]


def sort_date_for_entry(entry: LifecycleEntry) -> str:
    return date_for_source(entry.family, Path(entry.source_path)) or ""


def entry_sort_key(entry: LifecycleEntry) -> tuple[int, str, str]:
    # Descending by date, then by source path.
    sort_date = sort_date_for_entry(entry)
    inverted = "".join(chr(255 - ord(ch)) for ch in sort_date)
    return (FAMILY_ORDER.get(entry.family, 99), inverted, entry.source_path)


def sort_entries(entries: Iterable[LifecycleEntry]) -> List[LifecycleEntry]:
    return sorted(entries, key=entry_sort_key)


def date_for_source(family: str, source_path: Path) -> str | None:
    if family == "history":
        stem = source_path.stem
        if DATE_RE.match(stem):
            return stem
        return None
    if family == "dated_review":
        if len(source_path.parts) >= 3 and DATE_RE.match(source_path.parts[2]):
            return source_path.parts[2]
        return None
    if family == "superseded_milestone":
        if len(source_path.parts) >= 3 and DATE_RE.match(source_path.parts[2]):
            return source_path.parts[2]
        return None
    return None


def family_for_source(source_path: Path) -> str | None:
    parts = source_path.parts
    if len(parts) == 3 and parts[:2] == ("docs", "history") and source_path.suffix == ".md" and DATE_RE.match(source_path.stem):
        return "history"
    if len(parts) >= 4 and parts[:2] == ("docs", "review") and DATE_RE.match(parts[2]) and source_path.suffix == ".md":
        return "dated_review"
    if len(parts) >= 4 and parts[:2] == ("docs", "milestones") and DATE_RE.match(parts[2]) and source_path.suffix == ".md":
        text = repo_abspath(source_path).read_text(encoding="utf-8") if repo_abspath(source_path).exists() else ""
        if "**Status:** Superseded draft" in text:
            return "superseded_milestone"
    return None


def archive_path_for(source_path: Path, family: str) -> Path:
    if family == "history":
        return Path("docs/archive/history") / source_path.name
    if family == "dated_review":
        return Path("docs/archive/review") / Path(*source_path.parts[2:])
    if family == "superseded_milestone":
        return Path("docs/archive/milestones") / Path(*source_path.parts[2:])
    raise ValueError(f"unsupported family for archive mapping: {family}")


def is_archive_path(path: Path) -> bool:
    return path.parts[:2] == ("docs", "archive")


def is_markdown_doc(path: Path) -> bool:
    return path.suffix == ".md"


def history_sources() -> List[Path]:
    base = DOCS / "history"
    return sorted(
        [path.relative_to(ROOT) for path in base.glob("*.md") if DATE_RE.match(path.stem)],
        key=lambda path: path.name,
        reverse=True,
    )


def dated_review_sources() -> List[Path]:
    results: List[Path] = []
    base = DOCS / "review"
    for child in sorted(base.iterdir(), key=lambda item: item.name, reverse=True):
        if not child.is_dir() or not DATE_RE.match(child.name):
            continue
        for path in sorted(child.glob("*.md")):
            results.append(path.relative_to(ROOT))
    return results


def keep_window_dirs_for_dated_review() -> List[str]:
    dirs = [path.name for path in sorted((DOCS / "review").iterdir(), key=lambda item: item.name, reverse=True) if path.is_dir() and DATE_RE.match(path.name)]
    manifest = load_manifest() if MANIFEST_PATH.exists() else default_manifest()
    keep = int(manifest["keep_window"]["dated_review"])  # type: ignore[index]
    return dirs[:keep]


def keep_set_for_family(family: str, manifest: Dict[str, object]) -> set[str]:
    keep_window = manifest.get("keep_window", {})
    if family == "history":
        keep = int(keep_window.get("history", 0))
        return {path.as_posix() for path in history_sources()[:keep]}
    if family == "dated_review":
        keep = int(keep_window.get("dated_review", 0))
        keep_dates = [
            path.name
            for path in sorted((DOCS / "review").iterdir(), key=lambda item: item.name, reverse=True)
            if path.is_dir() and DATE_RE.match(path.name)
        ][:keep]
        return {
            path.as_posix()
            for path in dated_review_sources()
            if len(path.parts) >= 3 and path.parts[2] in keep_dates
        }
    if family == "superseded_milestone":
        keep = int(keep_window.get("superseded_milestone", 0))
        if keep <= 0:
            return set()
    return set()


def ledger_text(manifest: Dict[str, object]) -> str:
    last_updated = str(manifest.get("last_updated", TODAY))
    entries = sort_entries(entries_from_manifest(manifest))

    lines = [
        "# Archive Ledger",
        "",
        "**Purpose:** Provide the generated provenance ledger for summarized or archived docs; the JSON source of truth is [ARCHIVE-MANIFEST.json](./ARCHIVE-MANIFEST.json).",
        "",
        f"**Last updated:** {last_updated}",
        "",
        "> Generated by `python3 scripts/docs-lifecycle.py validate`. Do not hand-edit this ledger; update the JSON manifest through lifecycle commands instead.",
        "",
        "## Rules",
        "",
        "1. `archived` means the active raw source has been moved under `docs/archive/` without content rewriting.",
        "2. `summarized_active` means the raw source still stays in the active tree because it is within the keep window, but its key value has already been folded into active docs.",
        "3. Provenance lives here; `docs/rollups/*.md` stays concise and does not list source files inline.",
        "",
        "## Entries",
        "",
        "| Source Path | Family | Theme | Extracted Value | Target Docs | Status | Archive Path | Archived At |",
        "|-------------|--------|-------|-----------------|-------------|--------|--------------|-------------|",
    ]

    if not entries:
        lines.append("| *(empty)* | - | - | - | - | - | - | - |")
    else:
        for entry in entries:
            targets = "<br>".join(entry.targets) if entry.targets else "-"
            theme = entry.theme or "-"
            value = entry.value or "-"
            archived_at = entry.archived_at or "-"
            lines.append(
                f"| `{entry.source_path}` | `{entry.family}` | `{escape_table(theme)}` | {escape_table(value)} | {escape_table(targets)} | `{entry.status}` | `{entry.archive_path}` | `{archived_at}` |"
            )

    lines.append("")
    return "\n".join(lines)


def escape_table(text: str) -> str:
    return text.replace("|", "\\|")


def write_ledger(manifest: Dict[str, object]) -> None:
    LEDGER_PATH.parent.mkdir(parents=True, exist_ok=True)
    LEDGER_PATH.write_text(ledger_text(manifest), encoding="utf-8")


def ensure_targets(paths: Sequence[str]) -> tuple[str, ...]:
    targets: List[str] = []
    for raw in paths:
        path = normalize_repo_path(raw)
        if is_archive_path(path):
            raise RuntimeError(f"target cannot live under docs/archive/: {raw}")
        absolute = repo_abspath(path)
        if not absolute.exists():
            raise RuntimeError(f"missing target doc: {path.as_posix()}")
        if not absolute.is_file() or absolute.suffix != ".md":
            raise RuntimeError(f"target must be an existing markdown file: {path.as_posix()}")
        targets.append(path.as_posix())
    return tuple(targets)


def render_candidates_tree(entries: Sequence[LifecycleEntry]) -> str:
    root: Dict[str, object] = {
        "name": ".",
        "type": "directory",
        "path": ".",
        "children": {},
    }
    for entry in entries:
        segments = Path(entry.source_path).parts
        node = root
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
        node["children"][segments[-1]] = {  # type: ignore[index]
            "name": segments[-1],
            "type": "file",
            "entry": entry,
        }

    def freeze(node: Dict[str, object]) -> Dict[str, object]:
        if node["type"] != "directory":
            return node
        frozen_children = [freeze(child) for child in node["children"].values()]  # type: ignore[index]
        frozen_children.sort(key=lambda child: (child["type"] != "directory", str(child["name"]).lower()))
        return {
            "name": node["name"],
            "type": node["type"],
            "children": frozen_children,
        }

    def label_for(entry: LifecycleEntry) -> str:
        if entry.status == "archived":
            return f"{Path(entry.source_path).name} [{entry.status} -> {entry.archive_path}]"
        if entry.status == "summarized_active":
            return f"{Path(entry.source_path).name} [{entry.status}]"
        if entry.status == "pending_archive":
            return f"{Path(entry.source_path).name} [{entry.status} -> {entry.archive_path}]"
        return f"{Path(entry.source_path).name} [{entry.status}]"

    lines = ["."]

    def visit(children: Sequence[Dict[str, object]], prefix: str) -> None:
        for index, child in enumerate(children):
            is_last = index == len(children) - 1
            branch = "└── " if is_last else "├── "
            if child["type"] == "directory":
                lines.append(f"{prefix}{branch}{child['name']}/")
                next_prefix = prefix + ("    " if is_last else "│   ")
                visit(child["children"], next_prefix)  # type: ignore[arg-type]
                continue
            entry = child["entry"]  # type: ignore[index]
            lines.append(f"{prefix}{branch}{label_for(entry)}")

    visit(freeze(root)["children"], "")  # type: ignore[arg-type]
    return "\n".join(lines)


def render_candidates_list(entries: Sequence[LifecycleEntry]) -> str:
    if not entries:
        return "(empty)"
    lines: List[str] = []
    for entry in entries:
        extra = f" archive={entry.archive_path}" if entry.status in {"archived", "pending_archive"} else ""
        targets = f" targets={','.join(entry.targets)}" if entry.targets else ""
        theme = f" theme={entry.theme}" if entry.theme else ""
        lines.append(f"- {entry.source_path} [{entry.status}]{theme}{targets}{extra}")
    return "\n".join(lines)


def render_candidates_json(entries: Sequence[LifecycleEntry], family: str) -> str:
    counts = {
        "active": sum(1 for entry in entries if entry.status == "active"),
        "summarized_active": sum(1 for entry in entries if entry.status == "summarized_active"),
        "pending_archive": sum(1 for entry in entries if entry.status == "pending_archive"),
        "archived": sum(1 for entry in entries if entry.status == "archived"),
    }
    payload = {
        "root": ROOT.as_posix(),
        "family": family,
        "counts": counts,
        "entries": [
            {
                "source_path": entry.source_path,
                "family": entry.family,
                "status": entry.status,
                "theme": entry.theme,
                "value": entry.value,
                "targets": list(entry.targets),
                "archive_path": entry.archive_path,
                "archived_at": entry.archived_at,
            }
            for entry in entries
        ],
    }
    return json.dumps(payload, ensure_ascii=False, indent=2)


def discover_candidates(family: str, manifest: Dict[str, object]) -> List[LifecycleEntry]:
    manifest_entries = {entry.source_path: entry for entry in entries_from_manifest(manifest) if entry.family in ACTIVE_FAMILIES}

    discovered: Dict[str, LifecycleEntry] = {}
    if family in {"history", "all"}:
        for path in history_sources():
            discovered[path.as_posix()] = LifecycleEntry(
                source_path=path.as_posix(),
                family="history",
                status="active",
                archive_path=archive_path_for(path, "history").as_posix(),
            )
    if family in {"dated_review", "all"}:
        for path in dated_review_sources():
            discovered[path.as_posix()] = LifecycleEntry(
                source_path=path.as_posix(),
                family="dated_review",
                status="active",
                archive_path=archive_path_for(path, "dated_review").as_posix(),
            )

    for source_path, entry in manifest_entries.items():
        if family != "all" and entry.family != family:
            continue
        discovered[source_path] = entry

    return sort_entries(discovered.values())


def run_candidates(args: argparse.Namespace) -> int:
    manifest = load_manifest() if MANIFEST_PATH.exists() else default_manifest()
    entries = discover_candidates(args.family, manifest)
    counts = {
        "active": sum(1 for entry in entries if entry.status == "active"),
        "summarized_active": sum(1 for entry in entries if entry.status == "summarized_active"),
        "pending_archive": sum(1 for entry in entries if entry.status == "pending_archive"),
        "archived": sum(1 for entry in entries if entry.status == "archived"),
    }
    header = [
        f"family: {args.family}",
        f"selected: {len(entries)} (active={counts['active']}, summarized_active={counts['summarized_active']}, pending_archive={counts['pending_archive']}, archived={counts['archived']})",
        "",
    ]

    if args.format == "tree":
        print("\n".join(header) + render_candidates_tree(entries))
    elif args.format == "list":
        print("\n".join(header) + render_candidates_list(entries))
    elif args.format == "json":
        print(render_candidates_json(entries, args.family))
    else:
        raise RuntimeError(f"unsupported format: {args.format}")
    return 0


def run_mark(args: argparse.Namespace) -> int:
    manifest = ensure_manifest_exists()
    source = normalize_repo_path(args.source)
    if is_archive_path(source):
        raise RuntimeError(f"source cannot be under docs/archive/: {source.as_posix()}")
    absolute_source = repo_abspath(source)
    if not absolute_source.exists():
        raise RuntimeError(f"missing source doc: {source.as_posix()}")
    if absolute_source.suffix != ".md":
        raise RuntimeError(f"source must be a markdown doc: {source.as_posix()}")

    family = family_for_source(source)
    if family not in ACTIVE_FAMILIES:
        raise RuntimeError(f"unsupported lifecycle source: {source.as_posix()}")

    existing = {entry.source_path for entry in entries_from_manifest(manifest)}
    if source.as_posix() in existing:
        raise RuntimeError(f"duplicate source already tracked in lifecycle manifest: {source.as_posix()}")

    value = args.value.strip()
    if not value or "\n" in value:
        raise RuntimeError("--value must be a single non-empty line")

    targets = ensure_targets(args.target)
    keep_set = keep_set_for_family(family, manifest)
    status = "summarized_active" if source.as_posix() in keep_set else "pending_archive"
    entry = LifecycleEntry(
        source_path=source.as_posix(),
        family=family,
        status=status,
        archive_path=archive_path_for(source, family).as_posix(),
        theme=args.theme.strip(),
        value=value,
        targets=targets,
    )

    entries = entries_from_manifest(manifest)
    entries.append(entry)
    manifest["entries"] = [item.to_dict() for item in sort_entries(entries)]
    manifest["last_updated"] = TODAY
    save_manifest(manifest)
    write_ledger(manifest)
    print(f"marked {source.as_posix()} as {status}")
    return 0


def is_active_doc_for_cleanup(path: Path, excluded_sources: set[str]) -> bool:
    if path.as_posix() in excluded_sources:
        return False
    if is_archive_path(path):
        return False
    if path.name == "INDEX.md":
        return False
    return path.suffix == ".md"


def link_targets_for_doc(path: Path) -> List[Path]:
    text = repo_abspath(path).read_text(encoding="utf-8")
    targets: List[Path] = []
    for raw_target in MARKDOWN_LINK_RE.findall(text):
        target = raw_target.strip()
        if not target or target.startswith("http://") or target.startswith("https://") or target.startswith("mailto:") or target.startswith("#"):
            continue
        target = target.split("#", 1)[0]
        if not target or re.search(r"\s", target):
            continue
        if not target.startswith("./") and not target.startswith("../") and not target.endswith(".md"):
            continue
        resolved = (repo_abspath(path).parent / target).resolve()
        try:
            targets.append(resolved.relative_to(ROOT))
        except ValueError:
            continue
    return targets


def find_direct_links_to_sources(sources: Sequence[LifecycleEntry]) -> List[str]:
    tracked_sources = {entry.source_path for entry in sources}
    errors: List[str] = []
    for path in sorted(DOCS.rglob("*.md")):
        rel = path.relative_to(ROOT)
        if not is_active_doc_for_cleanup(rel, tracked_sources):
            continue
        targets = link_targets_for_doc(rel)
        for target in targets:
            if target.as_posix() in tracked_sources:
                errors.append(f"{rel.as_posix()} -> {target.as_posix()}")
    return errors


def remove_empty_parents(path: Path, stop: Path) -> None:
    current = path
    while current != stop and current.exists():
        try:
            current.rmdir()
        except OSError:
            return
        current = current.parent


def write_indexes() -> None:
    proc = subprocess.run(
        [sys.executable, str(ROOT / "scripts/docs-index.py"), "--write"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        raise RuntimeError(f"failed to regenerate indexes: {detail}")


def run_cleanup(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    entries = entries_from_manifest(manifest)
    pending = [
        entry
        for entry in entries
        if entry.status == "pending_archive" and (args.family == "all" or entry.family == args.family)
    ]
    if not pending:
        print("nothing to archive")
        return 0

    link_conflicts = find_direct_links_to_sources(pending)
    if link_conflicts:
        raise RuntimeError(
            "active docs still link to pending-archive sources:\n" + "\n".join(f"  - {item}" for item in link_conflicts)
        )

    updated: List[LifecycleEntry] = []
    pending_map = {entry.source_path: entry for entry in pending}
    for entry in entries:
        if entry.source_path not in pending_map:
            updated.append(entry)
            continue
        expected_archive = archive_path_for(Path(entry.source_path), entry.family).as_posix()
        if entry.archive_path != expected_archive:
            raise RuntimeError(
                f"archive path mismatch for {entry.source_path}: expected {expected_archive}, got {entry.archive_path}"
            )
        source_abs = repo_abspath(Path(entry.source_path))
        archive_abs = repo_abspath(Path(entry.archive_path))
        if not source_abs.exists():
            raise RuntimeError(f"pending source is missing before cleanup: {entry.source_path}")
        if archive_abs.exists():
            raise RuntimeError(f"archive target already exists: {entry.archive_path}")
        archive_abs.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(source_abs), str(archive_abs))
        remove_empty_parents(source_abs.parent, DOCS)
        updated.append(
            LifecycleEntry(
                source_path=entry.source_path,
                family=entry.family,
                status="archived",
                archive_path=entry.archive_path,
                theme=entry.theme,
                value=entry.value,
                targets=entry.targets,
                archived_at=TODAY,
            )
        )

    manifest["entries"] = [entry.to_dict() for entry in sort_entries(updated)]
    manifest["last_updated"] = TODAY
    save_manifest(manifest)
    write_ledger(manifest)
    write_indexes()
    print(f"archived {len(pending)} source docs")
    return 0


def validate_targets(entry: LifecycleEntry, errors: List[str]) -> None:
    for target in entry.targets:
        path = normalize_repo_path(target)
        if is_archive_path(path):
            errors.append(f"target must stay active, not archived: {entry.source_path} -> {target}")
            continue
        absolute = repo_abspath(path)
        if not absolute.exists():
            errors.append(f"missing target doc: {entry.source_path} -> {target}")
            continue
        if absolute.suffix != ".md":
            errors.append(f"target is not markdown: {entry.source_path} -> {target}")


def run_validate(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    errors: List[str] = []

    if int(manifest.get("version", 0)) != 1:
        errors.append("archive manifest version must be 1")
    if manifest.get("archive_root") != "docs/archive":
        errors.append("archive_root must be docs/archive")
    if manifest.get("rollup_root") != "docs/rollups":
        errors.append("rollup_root must be docs/rollups")

    entries = entries_from_manifest(manifest)
    keep_sets = {
        family: keep_set_for_family(family, manifest)
        for family in FAMILY_ORDER
    }

    seen_sources: set[str] = set()
    for entry in entries:
        if entry.source_path in seen_sources:
            errors.append(f"duplicate source entry in lifecycle manifest: {entry.source_path}")
            continue
        seen_sources.add(entry.source_path)

        if entry.family not in FAMILY_ORDER:
            errors.append(f"unknown family: {entry.source_path} -> {entry.family}")
            continue
        if entry.status not in ALLOWED_STATUSES:
            errors.append(f"unknown status: {entry.source_path} -> {entry.status}")
        if entry.archive_path != archive_path_for(Path(entry.source_path), entry.family).as_posix():
            errors.append(f"archive path mismatch: {entry.source_path} -> {entry.archive_path}")

        source_abs = repo_abspath(Path(entry.source_path))
        archive_abs = repo_abspath(Path(entry.archive_path))
        is_non_active = entry.status != "active"
        if is_non_active:
            if not entry.theme:
                errors.append(f"missing theme for non-active entry: {entry.source_path}")
            if not entry.value:
                errors.append(f"missing extracted value for non-active entry: {entry.source_path}")
            if not entry.targets:
                errors.append(f"missing targets for non-active entry: {entry.source_path}")
            else:
                validate_targets(entry, errors)

        if entry.status == "archived":
            if source_abs.exists():
                errors.append(f"archived entry source still exists: {entry.source_path}")
            if not archive_abs.exists():
                errors.append(f"archived entry archive target missing: {entry.archive_path}")
            if entry.archived_at is None:
                errors.append(f"archived entry missing archived_at: {entry.source_path}")
        elif entry.status in {"pending_archive", "summarized_active", "active"}:
            if not source_abs.exists():
                errors.append(f"active-side entry source missing: {entry.source_path}")
            if archive_abs.exists():
                errors.append(f"non-archived entry already has archive target present: {entry.archive_path}")
            if entry.archived_at is not None:
                errors.append(f"non-archived entry should not have archived_at: {entry.source_path}")

        if entry.status == "summarized_active" and entry.source_path not in keep_sets.get(entry.family, set()):
            errors.append(f"summarized_active entry falls outside keep window: {entry.source_path}")
        if entry.status == "pending_archive" and entry.source_path in keep_sets.get(entry.family, set()):
            errors.append(f"pending_archive entry still falls inside keep window: {entry.source_path}")

    expected_ledger = ledger_text(manifest)
    if not LEDGER_PATH.exists():
        errors.append(f"missing archive ledger: {LEDGER_PATH.relative_to(ROOT)}")
    else:
        current_ledger = LEDGER_PATH.read_text(encoding="utf-8")
        if current_ledger != expected_ledger:
            errors.append(f"archive ledger is stale: {LEDGER_PATH.relative_to(ROOT)}")

    if errors:
        print("docs lifecycle validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("docs lifecycle validation passed")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Manage doc compression, rollups, and archive lifecycle state.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    candidates = subparsers.add_parser("candidates", help="List lifecycle candidates and their current status.")
    candidates.add_argument("--family", choices=["history", "dated_review", "all"], required=True)
    candidates.add_argument("--format", choices=["tree", "list", "json"], default="tree")
    candidates.set_defaults(func=run_candidates)

    mark = subparsers.add_parser("mark", help="Record a summarized source doc in the lifecycle manifest.")
    mark.add_argument("--source", required=True)
    mark.add_argument("--theme", required=True)
    mark.add_argument("--value", required=True)
    mark.add_argument("--target", action="append", required=True)
    mark.set_defaults(func=run_mark)

    cleanup = subparsers.add_parser("cleanup", help="Archive pending sources once they are safe to move.")
    cleanup.add_argument("--family", choices=["history", "dated_review", "all"], required=True)
    cleanup.set_defaults(func=run_cleanup)

    validate = subparsers.add_parser("validate", help="Validate archive manifest, ledger, and source/archive layout.")
    validate.set_defaults(func=run_validate)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return args.func(args)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
