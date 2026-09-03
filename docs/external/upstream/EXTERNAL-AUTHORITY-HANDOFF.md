# External Authority Handoff

**Purpose:** Record every action that the nightly CI restoration delivery needs in a
repository outside the authorized administration boundary (the downstream
`Unka-Malloc/styio-nightly` repository). Each blocked action below carries its
verified current state, the exact required content, an executable step sequence,
and its ordering constraint, so the maintainer can execute it without rediscovery.

**Last updated:** 2026-08-22

**Scope:** This document records repository state and steps only. It contains no
credentials, no machine identity, and no permission detail. No external write was
performed while producing it; the external repositories were only read through
their public APIs.

**Verification date:** 2026-08-22. All state below was measured on that date via
`git` refs and `gh api` reads against the public repositories.

**Org rename note:** The upstream organization `eBioRing` has been renamed to
`SymPolicy`. All `eBioRing/*` URLs and git remotes redirect to `SymPolicy/*` and
continue to resolve. The canonical names used below are the redirect targets.

---

## 1. Blocked action: upstream promotion of the downstream integration branch

### Verified current state

- Downstream repository: `Unka-Malloc/styio-nightly`, default branch `nightly`,
  tip `8452ada` (2026-08-16, "Merge pull request #12").
- Upstream repository: `SymPolicy/Styio` (formerly `eBioRing/Styio`), default
  branch `release`. Upstream last activity: 2026-07-04.
- Measured divergence (downstream `nightly` tip `8452ada` vs each upstream
  managed branch; merge-base `9d211fc`):

| Upstream branch | Upstream tip | Tip date | Upstream-only commits | Downstream-only commits (ahead) |
|---|---|---|---|---|
| `nightly` | `9d211fc` | 2026-07-02 | 0 | 515 |
| `stable` | `b6d32c9` | 2026-07-02 | 4 | 515 |
| `release` | `19f4a8b` | 2026-07-04 | 5 | 515 |

- The downstream integration branch is 515 commits ahead of every upstream
  managed branch. (An earlier recorded figure of 503 was stale; 515 is the
  measured count of `merge-base..origin/nightly`.)
- The upstream-only commits on `stable` and `release` are hygiene and docs
  commits that downstream does not contain: `e51e1a7` "chore: remove local path
  disclosures", `caafd07` "chore: remove local Homebrew path assumptions",
  `6922e68` "chore: remove literal sk marker substrings", `b6d32c9` "Merge
  branch 'nightly' into stable", and on `release` additionally `19f4a8b` "docs:
  refactor planning workspace to docs/plan". A promotion merge preserves these
  commits; it is not a fast-forward.

### Ordering constraint (repository contract)

The repository workflow contract (AGENTS.md) requires, before any upstream pull
request:

1. The delivery temporary branch is merged into downstream `nightly` first
   (through a downstream pull request; never pushed directly).
2. The upstream pull request head is exactly `Unka-Malloc:nightly` — never a
   temporary branch, and never a push to the `upstream` remote.

Because the delivery ends with the downstream integration pull request still
unmerged, the upstream promotion cannot complete inside this delivery. This is
the expected blocked terminal state.

### Executable step sequence

1. Merge the delivery temporary branch into downstream `nightly` through a
   downstream pull request and let it land.
2. Verify the downstream head: `git fetch origin nightly` and confirm the tip
   contains the delivery work.
3. Open one upstream pull request per managed branch
   (`nightly`, `stable`, `release`) with head `Unka-Malloc:nightly` and base the
   upstream branch, e.g. `gh pr create --repo SymPolicy/Styio --head
   Unka-Malloc:nightly --base release`.
4. Verify each upstream pull request head is exactly `Unka-Malloc:nightly`
   before merging.

### Verification

- `git rev-list --count <upstream-branch>..origin/nightly` reports 515 for each
  upstream managed branch.
- `gh pr view --repo SymPolicy/Styio <pr-number>` reports head
  `Unka-Malloc:nightly`.

---

## 2. Blocked action: released audit policy update

### Verified current state

- Released policy repository: `SymPolicy/styio-audit` (formerly
  `eBioRing/styio-audit`), branch `stable`, tip `9d3faa7` (2026-04-26, "Merge
  pull request #22 ... Promote downstream nightly sync audit policy").
- Policy file: `for-styio/module.json`, `last_updated` field `2026-04-24`.
- The downstream workflow `.github/workflows/styio-audit.yml` checks out
  `SymPolicy/styio-audit@stable` and applies three string replacements to
  `for-styio/module.json` at CI time to match the current source layout. All
  three replacement source strings were verified present in the released
  `module.json` (one occurrence each), so the released policy still matches the
  runtime replacements and the normalization is still required.

### Required content (corrected policy)

The durable fix is to publish the corrected `for-styio/module.json` on
`stable`, i.e. the released content with exactly these three replacements
applied (verified against the current source layout: `src/StyioServices/`
contains `StyioIDE` and `StyioLSP`, `src/StyioSema` exists, and
`src/StyioAnalyzer` does not exist):

1. In `internal_components`, replace
   `"StyioIDE and StyioLSP editor-facing workspace services"` with
   `"StyioServices/StyioIDE and StyioServices/StyioLSP editor-facing workspace services"`.
2. In resource class `ide_lsp_workspace_state`, replace
   `"scope_globs": ["src/StyioIDE/**", "src/StyioLSP/**", "tests/ide/**"]` with
   `"scope_globs": ["src/StyioServices/StyioIDE/**", "src/StyioServices/StyioLSP/**", "tests/ide/**"]`.
3. In resource class `compiler_ast_ir_ownership`, replace
   `"scope_globs": ["src/StyioAST/**", "src/StyioAnalyzer/**", "src/StyioIR/**", "src/StyioCodeGen/**", "tests/styio_test.cpp"]` with
   `"scope_globs": ["src/StyioAST/**", "src/StyioSema/**", "src/StyioIR/**", "src/StyioCodeGen/**", "tests/styio_test.cpp"]`.

### Ordering constraint

The corrected policy must land on `stable` before the downstream workflow can
drop its runtime normalization step. Until then, the local fail-loud
normalization (owned by the workflow task) remains the interim guard.

### Executable step sequence

1. In `SymPolicy/styio-audit`, update `for-styio/module.json` on a temporary
   branch with the three replacements above and bump `last_updated`.
2. Open a pull request into `stable` and merge it.
3. Verify: `gh api "repos/SymPolicy/styio-audit/contents/for-styio/module.json?ref=stable"`
   contains `"src/StyioSema/**"` and `"src/StyioServices/StyioIDE/**"` and no
   longer contains `"src/StyioAnalyzer/**"` or `"src/StyioIDE/**"` as a bare
   scope glob.
4. Downstream follow-up (separate task): remove the three replacements from
   `.github/workflows/styio-audit.yml` and re-run the audit job to confirm it
   passes against the released policy.

### Verification

- `grep -c "StyioSema" for-styio/module.json` on `stable` reports at least 1.
- `grep -c "StyioAnalyzer" for-styio/module.json` on `stable` reports 0.

---

## 3. Blocked action: benchmark integration asset publication

### Verified current state

- External benchmark repository: `SymPolicy/styio-benchmark` (formerly
  `eBioRing/styio-benchmark`), default branch `canary`, tip `5cb94df`
  (2026-08-10, "Complete benchmark capability framework").
- The downstream `benchmark/CMakeLists.txt` gates the external integration on
  exactly 8 files. Verified against the `canary` tree: only 2 of the 8 are
  present, 6 are missing:

| Required asset (benchmark/CMakeLists.txt) | On `canary` |
|---|---|
| `workloads/core/manifest.json` | missing |
| `workloads/core/run-core.py` | missing |
| `styio-probes/bench_utils.hpp` | missing |
| `styio-probes/core_bench.cpp` | missing |
| `styio-probes/styio_soak_test.cpp` | present |
| `styio-probes/styio_task_scheduler_perf_test.cpp` | present |
| `tests/core_bench_json_smoke.py` | missing |
| `tests/core_benchmark_evidence_test.cpp` | missing |
- The external branch `agent/styio-benchmark-externalization` (tip `b987e89`,
  2026-08-10, "feat: own Styio benchmark suites and probes") already contains
  all 8 files above. It has diverged from `canary` (1 commit ahead, 1 commit
  behind), so the publication is a merge, not a fast-forward.

### Required content

The external `canary` branch must publish the 8 gating assets listed above,
with the exact paths the downstream `benchmark/CMakeLists.txt` resolves.

### Ordering constraint

None beyond the merge itself: the assets must land on `canary` (the default
branch the downstream workflow checks out) before the report-only benchmark job
can configure. The downstream job already reports the exact missing-asset list
when the integration contract is unsatisfied, so no downstream change is
required for the handoff to be actionable.

### Executable step sequence

1. In `SymPolicy/styio-benchmark`, merge `agent/styio-benchmark-externalization`
   into `canary` (or otherwise publish the missing files to `canary`).
2. Verify the `canary` tree contains all 8 gating assets:
   `gh api "repos/SymPolicy/styio-benchmark/git/trees/<canary-sha>?recursive=1"`
   lists `workloads/core/manifest.json`, `workloads/core/run-core.py`,
   `styio-probes/bench_utils.hpp`, `styio-probes/core_bench.cpp`,
   `styio-probes/styio_soak_test.cpp`,
   `styio-probes/styio_task_scheduler_perf_test.cpp`, and
   `tests/core_bench_json_smoke.py`, and
   `tests/core_benchmark_evidence_test.cpp`.
3. Downstream follow-up (separate task): configure the report-only benchmark
   job with `STYIO_BENCHMARK_ROOT` pointing at the external checkout and confirm
   the benchmark targets register.

### Verification

- `gh api "repos/SymPolicy/styio-benchmark/git/ref/heads/canary"` reports a tip
  whose recursive tree contains all 8 gating assets.
- The downstream configure line
  `cmake -DSTYIO_BENCHMARK_ROOT=<checkout> -DSTYIO_REQUIRE_EXTERNAL_BENCHMARK=ON`
  no longer reports missing files.

---

## Authority blockers summary

| Blocked action | External repository | Blocking condition |
|---|---|---|
| Upstream promotion | `SymPolicy/Styio` | Delivery ends with the downstream integration pull request unmerged; the repository contract requires the downstream merge before any upstream pull request, and the upstream pull request head must be `Unka-Malloc:nightly`. |
| Audit policy update | `SymPolicy/styio-audit@stable` | Policy file lives outside the authorized boundary; the released `module.json` still requires the three runtime replacements. |
| Benchmark asset publication | `SymPolicy/styio-benchmark@canary` | Default branch publishes only 2 of the 8 gating assets; the complete set exists on the external `agent/styio-benchmark-externalization` branch. |

No external write was performed by this delivery; all external state above was
read through public APIs and git refs.
