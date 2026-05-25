# Team Runbook Document Stats

**Purpose:** Record the current size of each `docs/teams/` runbook using the repository-local `scripts/docs-audit.py` word-count and character-count rules; this is a maintenance snapshot, not a quality target.

**Last updated:** 2026-05-26

## Counting Method

The numbers below use the same approximation as `scripts/docs-audit.py`:

1. Each Han character counts as 1 word.
2. Each contiguous ASCII word counts as 1 word.
3. Each non-whitespace symbol counts as 1 word.
4. `character_count` is the raw Markdown character length.

This table excludes generated [INDEX.md](./INDEX.md), collection [README.md](./README.md), and this statistics file from the team-runbook total.
Refresh this snapshot in the same commit whenever a team runbook changes, even if adjacent commits already staged related statistics.

Refresh command:

```bash
python3 scripts/docs-audit.py --manifest valid --format json --output /tmp/styio-docs.json
```

## Team Runbook Size

| Team | Document | Word count | Character count |
|------|----------|------------|-----------------|
| CLI / Nano | [CLI-NANO-RUNBOOK.md](./CLI-NANO-RUNBOOK.md) | 2,337 | 10,399 |
| Codegen / Runtime | [CODEGEN-RUNTIME-RUNBOOK.md](./CODEGEN-RUNTIME-RUNBOOK.md) | 2,633 | 12,334 |
| Coordination | [COORDINATION-RUNBOOK.md](./COORDINATION-RUNBOOK.md) | 2,029 | 7,587 |
| Docs / Ecosystem | [DOCS-ECOSYSTEM-RUNBOOK.md](./DOCS-ECOSYSTEM-RUNBOOK.md) | 5,633 | 26,549 |
| Frontend | [FRONTEND-RUNBOOK.md](./FRONTEND-RUNBOOK.md) | 3,523 | 14,968 |
| Grammar | [GRAMMAR-RUNBOOK.md](./GRAMMAR-RUNBOOK.md) | 933 | 3,774 |
| IDE / LSP | [IDE-LSP-RUNBOOK.md](./IDE-LSP-RUNBOOK.md) | 1,398 | 6,244 |
| Performance / Stability | [PERF-STABILITY-RUNBOOK.md](./PERF-STABILITY-RUNBOOK.md) | 1,888 | 8,141 |
| Sema / IR | [SEMA-IR-RUNBOOK.md](./SEMA-IR-RUNBOOK.md) | 3,440 | 15,789 |
| Test Quality | [TEST-QUALITY-RUNBOOK.md](./TEST-QUALITY-RUNBOOK.md) | 4,661 | 22,576 |
| **Total** | Team runbooks only | **28,475** | **128,361** |

## Support File Size

| Document | Word count | Character count |
|----------|------------|-----------------|
| [README.md](./README.md) | 599 | 2,295 |

Support files are not counted as team-owned runbooks. Their main purpose is collection maintenance and generated inventory navigation.
