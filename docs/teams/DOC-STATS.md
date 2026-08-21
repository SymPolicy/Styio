# Team Runbook Document Stats

**Purpose:** Record the current size of each `docs/teams/` runbook using the repository-local `scripts/docs-audit.py` word-count and character-count rules; this is a maintenance snapshot, not a quality target.

**Last updated:** 2026-08-22

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
| CLI / Nano | [CLI-NANO-RUNBOOK.md](./CLI-NANO-RUNBOOK.md) | 2,788 | 13,042 |
| Codegen / Runtime | [CODEGEN-RUNTIME-RUNBOOK.md](./CODEGEN-RUNTIME-RUNBOOK.md) | 2,734 | 13,231 |
| Coordination | [COORDINATION-RUNBOOK.md](./COORDINATION-RUNBOOK.md) | 2,049 | 7,640 |
| Docs / Ecosystem | [DOCS-ECOSYSTEM-RUNBOOK.md](./DOCS-ECOSYSTEM-RUNBOOK.md) | 5,546 | 27,113 |
| Frontend | [FRONTEND-RUNBOOK.md](./FRONTEND-RUNBOOK.md) | 6,355 | 28,144 |
| Grammar | [GRAMMAR-RUNBOOK.md](./GRAMMAR-RUNBOOK.md) | 1,020 | 4,089 |
| IDE / LSP | [IDE-LSP-RUNBOOK.md](./IDE-LSP-RUNBOOK.md) | 1,667 | 7,454 |
| Performance / Stability | [PERF-STABILITY-RUNBOOK.md](./PERF-STABILITY-RUNBOOK.md) | 3,053 | 13,676 |
| Sema / IR | [SEMA-IR-RUNBOOK.md](./SEMA-IR-RUNBOOK.md) | 9,568 | 46,747 |
| Test Quality | [TEST-QUALITY-RUNBOOK.md](./TEST-QUALITY-RUNBOOK.md) | 5,237 | 25,499 |
| **Total** | Team runbooks only | **40,017** | **186,735** |

## Support File Size

| Document | Word count | Character count |
|----------|------------|-----------------|
| [README.md](./README.md) | 599 | 2,295 |

Support files are not counted as team-owned runbooks. Their main purpose is collection maintenance and generated inventory navigation.
