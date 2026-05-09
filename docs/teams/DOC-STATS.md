# Team Runbook Document Stats

**Purpose:** Record the current size of each `docs/teams/` runbook using the repository-local `scripts/docs-audit.py` word-count and character-count rules; this is a maintenance snapshot, not a quality target.

**Last updated:** 2026-05-09

## Counting Method

The numbers below use the same approximation as `scripts/docs-audit.py`:

1. Each Han character counts as 1 word.
2. Each contiguous ASCII word counts as 1 word.
3. Each non-whitespace symbol counts as 1 word.
4. `character_count` is the raw Markdown character length.

This table excludes generated [INDEX.md](./INDEX.md), collection [README.md](./README.md), and this statistics file from the team-runbook total.

Refresh command:

```bash
python3 scripts/docs-audit.py --manifest valid --format json --output /tmp/styio-docs.json
```

## Team Runbook Size

| Team | Document | Word count | Character count |
|------|----------|------------|-----------------|
| CLI / Nano | [CLI-NANO-RUNBOOK.md](./CLI-NANO-RUNBOOK.md) | 1,664 | 7,439 |
| Codegen / Runtime | [CODEGEN-RUNTIME-RUNBOOK.md](./CODEGEN-RUNTIME-RUNBOOK.md) | 1,702 | 7,696 |
| Coordination | [COORDINATION-RUNBOOK.md](./COORDINATION-RUNBOOK.md) | 2,056 | 7,685 |
| Docs / Ecosystem | [DOCS-ECOSYSTEM-RUNBOOK.md](./DOCS-ECOSYSTEM-RUNBOOK.md) | 3,208 | 14,762 |
| Frontend | [FRONTEND-RUNBOOK.md](./FRONTEND-RUNBOOK.md) | 1,733 | 6,693 |
| Grammar | [GRAMMAR-RUNBOOK.md](./GRAMMAR-RUNBOOK.md) | 841 | 3,371 |
| IDE / LSP | [IDE-LSP-RUNBOOK.md](./IDE-LSP-RUNBOOK.md) | 1,084 | 4,637 |
| Performance / Stability | [PERF-STABILITY-RUNBOOK.md](./PERF-STABILITY-RUNBOOK.md) | 1,872 | 8,097 |
| Sema / IR | [SEMA-IR-RUNBOOK.md](./SEMA-IR-RUNBOOK.md) | 1,975 | 8,954 |
| Test Quality | [TEST-QUALITY-RUNBOOK.md](./TEST-QUALITY-RUNBOOK.md) | 2,399 | 10,966 |
| **Total** | Team runbooks only | **18,534** | **80,300** |

## Support File Size

| Document | Word count | Character count |
|----------|------------|-----------------|
| [README.md](./README.md) | 595 | 2,307 |

Support files are not counted as team-owned runbooks. Their main purpose is collection maintenance and generated inventory navigation.
