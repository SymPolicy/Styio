#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CollectionSpec:
    path: str
    index_title: str
    index_purpose: str


COLLECTION_SPECS: tuple[CollectionSpec, ...] = (
    CollectionSpec("docs", "Docs Index", "Provide the generated inventory for `docs/`; directory boundaries and maintenance rules live in [README.md](./README.md)."),
    CollectionSpec("docs/rollups", "Rollups Index", "Provide the generated inventory for `docs/rollups/`; compressed active summaries and default loading order live in [README.md](./README.md)."),
    CollectionSpec("docs/archive", "Archive Index", "Provide the generated inventory for `docs/archive/`; provenance rules and archive lifecycle boundaries live in [README.md](./README.md)."),
    CollectionSpec("docs/archive/history", "Archive History Index", "Provide the generated inventory for `docs/archive/history/`; this is a generated entrypoint, not a retained raw-history store."),
    CollectionSpec("docs/archive/review", "Archive Review Index", "Provide the generated inventory for `docs/archive/review/`; this is a generated entrypoint, not a retained dated-review store."),
    CollectionSpec("docs/design", "Design Index", "Provide the generated inventory for `docs/design/`; document boundaries and naming rules live in [README.md](./README.md)."),
    CollectionSpec("docs/design/syntax", "Syntax Design Index", "Provide the generated inventory for `docs/design/syntax/`; compact syntax-reference boundaries live in [README.md](./README.md)."),
    CollectionSpec("docs/design/syntax/features", "Syntax Feature Index", "Provide the generated inventory for distributed syntax-feature SSOT documents; lifecycle, dependency, and composition rules live in [README.md](./README.md)."),
    CollectionSpec("docs/specs", "Specs Index", "Provide the generated inventory for `docs/specs/`; document boundaries and naming rules live in [README.md](./README.md)."),
    CollectionSpec("docs/specs/audit", "Audit Specs Index", "Provide the generated inventory for `docs/specs/audit/`; audit checklist ownership lives in [README.md](./README.md)."),
    CollectionSpec("docs/teams", "Teams Index", "Provide the generated inventory for `docs/teams/`; team daily-work boundaries and runbook rules live in [README.md](./README.md)."),
    CollectionSpec("docs/review", "Review Index", "Provide the generated inventory for `docs/review/`; document boundaries and naming rules live in [README.md](./README.md)."),
    CollectionSpec("docs/plan", "Plan Index", "Provide the generated inventory for `docs/plan/`; document boundaries and naming rules live in [README.md](./README.md)."),
    CollectionSpec("docs/external", "External Docs Index", "Provide the generated inventory for `docs/external/`; external handoff boundaries live in [README.md](./README.md)."),
    CollectionSpec("docs/external/for-ide", "For IDE Index", "Provide the generated inventory for `docs/external/for-ide/`; IDE embedding, LSP usage, and edit-time parser guidance live in [README.md](./README.md)."),
    CollectionSpec("docs/external/for-pafio", "For Pafio Index", "Provide the generated inventory for `docs/external/for-pafio/`; handoff boundaries for the system Styio compiler and Pafio project workflows live in [README.md](./README.md)."),
    CollectionSpec("docs/assets", "Assets Index", "Provide the generated inventory for `docs/assets/`; asset boundaries and reuse rules live in [README.md](./README.md)."),
    CollectionSpec("docs/assets/templates", "Template Assets Index", "Provide the generated inventory for `docs/assets/templates/`; template boundaries and reuse rules live in [README.md](./README.md)."),
    CollectionSpec("docs/adr", "ADR Index", "Provide the generated inventory for `docs/adr/`; decision-record conventions live in [README.md](./README.md)."),
    CollectionSpec("docs/audit", "Audit Index", "Provide the generated inventory for `docs/audit/`; transient defect records live in ignored `docs/audit/defects/` and are enforced by external `styio-audit` runs."),
    CollectionSpec("docs/history", "History Index", "Provide the generated inventory for `docs/history/`; recovery rules live in [README.md](./README.md)."),
    CollectionSpec("workflows", "Workflows Index", "Provide the generated inventory for root-level `workflows/`; reusable delivery workflows and repo-local skills live in [README.md](./README.md)."),
    CollectionSpec("workflows/skills", "Workflow Skills Index", "Provide the generated inventory for `workflows/skills/`; repo-local skills used by root workflows live in [README.md](./README.md)."),
)


def collection_dirs(root: Path | None = None) -> list[Path]:
    if root is None:
        return [Path(spec.path) for spec in COLLECTION_SPECS]
    return [root / spec.path for spec in COLLECTION_SPECS]


def collection_index_meta() -> dict[str, tuple[str, str]]:
    return {spec.path: (spec.index_title, spec.index_purpose) for spec in COLLECTION_SPECS}
