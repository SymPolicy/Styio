# Team Runbook Document Stats

**Purpose:** Record the current size of each `docs/teams/` runbook using the repository-local `scripts/docs-audit.py` word-count and character-count rules; this is a maintenance snapshot, not a quality target.

**Last updated:** 2026-10-22

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
| CLI / Nano | [CLI-NANO-RUNBOOK.md](./CLI-NANO-RUNBOOK.md) | 4,183 | 20,156 |
| Codegen / Runtime | [CODEGEN-RUNTIME-RUNBOOK.md](./CODEGEN-RUNTIME-RUNBOOK.md) | 4,442 | 21,600 |
| Coordination | [COORDINATION-RUNBOOK.md](./COORDINATION-RUNBOOK.md) | 2,384 | 9,439 |
| Docs / Ecosystem | [DOCS-ECOSYSTEM-RUNBOOK.md](./DOCS-ECOSYSTEM-RUNBOOK.md) | 3,459 | 15,457 |
| Frontend | [FRONTEND-RUNBOOK.md](./FRONTEND-RUNBOOK.md) | 6,654 | 29,624 |
| Grammar | [GRAMMAR-RUNBOOK.md](./GRAMMAR-RUNBOOK.md) | 1,256 | 5,331 |
| IDE / LSP | [IDE-LSP-RUNBOOK.md](./IDE-LSP-RUNBOOK.md) | 2,205 | 10,324 |
| Performance / Stability | [PERF-STABILITY-RUNBOOK.md](./PERF-STABILITY-RUNBOOK.md) | 3,782 | 17,718 |
| Sema / IR | [SEMA-IR-RUNBOOK.md](./SEMA-IR-RUNBOOK.md) | 10,953 | 53,679 |
| Test Quality | [TEST-QUALITY-RUNBOOK.md](./TEST-QUALITY-RUNBOOK.md) | 7,901 | 39,228 |
| **Total** | Team runbooks only | **47,219** | **222,556** |

## Support File Size

| Document | Word count | Character count |
|----------|------------|-----------------|
| [README.md](./README.md) | 599 | 2,295 |

Support files are not counted as team-owned runbooks. Their main purpose is collection maintenance and generated inventory navigation.
