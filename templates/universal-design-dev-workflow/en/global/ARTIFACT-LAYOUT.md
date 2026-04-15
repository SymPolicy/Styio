# Artifact layout and locale parity

**Authority:** Kit organization. The `en/` and `zh-CN/` trees use the **same relative paths and filenames** (mirror). Each tree contains its own **`global/`** folder.

## Kit root

```
templates/universal-design-dev-workflow/
├── ENTRY.md
├── README.md
├── en/
│   └── global/
└── zh-CN/
    └── global/
```

## Inside this locale folder (`en/`)

```
en/
├── global/
├── README.md
├── WORKFLOW-MAP.md
├── DOCUMENTATION-POLICY.md
├── CONTRIBUTOR-AND-AGENT-SPEC.md
├── DOMAIN-OR-PRODUCT-SPEC.md
├── OPEN-QUESTIONS-AND-HUMAN-INPUT.md
├── THIRD-PARTY.md
├── architecture/
├── milestones/
├── tests/
├── history/
└── prompts/
```

## Stable artifact IDs

| ID | Typical file |
|----|----------------|
| `WORKFLOW_MAP` | `WORKFLOW-MAP.md` |
| `DOC_POLICY` | `DOCUMENTATION-POLICY.md` |
| `CONTRIBUTOR_SPEC` | `CONTRIBUTOR-AND-AGENT-SPEC.md` |
| `DOMAIN_SPEC` | `DOMAIN-OR-PRODUCT-SPEC.md` |
| `OPEN_QUESTIONS` | `OPEN-QUESTIONS-AND-HUMAN-INPUT.md` |
| `THIRD_PARTY` | `THIRD-PARTY.md` |
| `MILESTONE_BATCH` | `milestones/<YYYY-MM-DD>/00-Milestone-Index.md` |
| `MILESTONE_SEGMENT` | `milestones/<YYYY-MM-DD>/M*.md` |
| `TEST_CATALOG` | `tests/TEST-CATALOG.md` |
| `HISTORY_DAY` | `history/YYYY-MM-DD.md` |
| `ADR` | `architecture/ADR-NNNN-*.md` |

## Adoption

| Pattern | Description |
|---------|-------------|
| A | Copy `en/` (with `global/`) into `docs/` in the product repo. |
| B | Keep `docs/en/` and `docs/zh-CN/` as parallel mirrors. |
| C | Vendor the kit; open root `ENTRY.md`. |

Template cross-links assume **this locale folder** is the documentation root.
