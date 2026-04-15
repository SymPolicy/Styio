# Milestone 11: Multi-Edit Incremental Syntax

**Purpose:** M11 验收测试与任务分解；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Last updated:** 2026-04-15

**Status:** Planned frozen acceptance batch

**Depends on:** Existing Tree-sitter syntax backend and `styio_lspd` document sync  
**Goal:** `textDocument/didChange` must preserve ordered incremental edits, VFS must apply them canonically, and `SyntaxParser` must reuse Tree-sitter state across multi-edit deltas without regressing current IDE APIs.

---

## Definitions

For implementation sequencing and non-acceptance details, see:

- [`../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md`](../../plans/IDE-Incremental-Edits-and-Semantic-Query-Cache-Implementation-Plan.md)
- [`../../for-ide/TREE-SITTER.md`](../../for-ide/TREE-SITTER.md)
- [`../../for-ide/LSP.md`](../../for-ide/LSP.md)

This milestone freezes the following boundary:

1. LSP incremental edits are ordered and preserved.
2. VFS is the canonical owner of text application.
3. Tree-sitter receives structured edit deltas, not just reconstructed whole-text diffs.

---

## Acceptance Tests

The concrete GoogleTest / transcript names below are part of the frozen acceptance surface for M11.

### T11.01 — LSP multi-change transcript applies edits in order

**Target:** new IDE/LSP test  
**Suggested name:** `StyioLspServer.AppliesMultipleIncrementalChangesInOrder`

Scenario:

1. `didOpen` a valid Styio file
2. Send one `didChange` notification containing multiple `contentChanges`
3. Use mixed insert/delete/replace edits
4. Verify the resulting buffer equals the expected final source

Acceptance:

- final buffer matches expected text
- diagnostics correspond to final text, not intermediate partial states

### T11.02 — VFS sequential range edits produce correct text

**Target:** new VFS unit test  
**Suggested name:** `StyioVfs.AppliesSequentialTextEdits`

Scenario:

1. Start from a known source text
2. Apply several ordered edits against the same version transition
3. Include overlapping or adjacent edit shapes

Acceptance:

- the resulting text is correct
- invalid ranges are rejected or surfaced deterministically

### T11.03 — Tree-sitter reuses prior tree across multi-edit delta

**Target:** new syntax test  
**Suggested name:** `StyioSyntaxParser.ReusesIncrementalTreeAcrossMultiEditDelta`

Scenario:

1. Parse an initial document
2. Apply a delta containing at least two non-identical edits
3. Reparse through the incremental path

Acceptance:

- `SyntaxSnapshot::reused_incremental_tree == true`
- CST remains valid
- diagnostics reflect final text

### T11.04 — Multi-edit result matches full parse

**Target:** new syntax equivalence test  
**Suggested name:** `StyioSyntaxParser.MultiEditIncrementalMatchesFullParse`

Scenario:

1. Parse a source incrementally using a multi-edit delta
2. Parse the final text from scratch

Acceptance:

- token ranges match
- node coverage is equivalent
- diagnostics match after normalization

### T11.05 — Existing IDE/LSP regressions stay green

Required existing tests:

1. `StyioIdeService.DocumentSymbolsHoverDefinitionAndCompletion`
2. `StyioSyntaxParser.UsesTreeSitterBackendWhenAvailable`
3. `StyioSyntaxParser.ReusesIncrementalTreeForSubsequentParses`
4. `StyioLspServer.HandlesInitializeOpenAndCompletion`

---

## Implementation Tasks

### Task 11.1 — Add structured text edit types
**Role:** IDE Agent  
**Files:** `src/StyioIDE/Common.*`, `src/StyioIDE/VFS.*`  
**Action:** Define `TextEdit` and `DocumentDelta`-style internal types used by VFS and syntax layers.  
**Verify:** Build succeeds.

### Task 11.2 — Parse ordered LSP content changes
**Role:** LSP Agent  
**Files:** `src/StyioLSP/Server.cpp`, `src/StyioIDE/Service.*`  
**Action:** Parse all `contentChanges`, including `range`-based changes, and forward them to the IDE service without collapsing to one text blob.  
**Verify:** T11.01 passes.

### Task 11.3 — Teach VFS to apply edit sequences
**Role:** VFS Agent  
**Files:** `src/StyioIDE/VFS.*`  
**Action:** Apply ordered edits against canonical text and expose the resulting snapshot transition.  
**Verify:** T11.02 passes.

### Task 11.4 — Upgrade Tree-sitter backend to structured multi-edit reuse
**Role:** Syntax Agent  
**Files:** `src/StyioIDE/Syntax.*`, `src/StyioIDE/TreeSitterBackend.*`  
**Action:** Apply each edit via `ts_tree_edit`, then reparse once against final text. Preserve existing fallback hierarchy.  
**Verify:** T11.03 and T11.04 pass.

### Task 11.5 — Keep current IDE contracts stable
**Role:** Syntax Agent  
**Files:** `src/StyioIDE/Syntax.*`, `src/StyioIDE/SemDB.*`  
**Action:** Maintain current `SyntaxSnapshot`-level API and diagnostics flow while adding delta-aware parsing.  
**Verify:** T11.05 passes.

### Task 11.6 — Update docs
**Role:** Doc Agent  
**Files:** `docs/for-ide/*.md`, `docs/plans/*.md`, `docs/milestones/2026-04-15/*.md`  
**Action:** Record the multi-edit data flow and verification commands.  
**Verify:** `python3 scripts/docs-audit.py` passes.

---

## Performance Gates

M11 is not complete unless these gates hold:

1. 5k-line file, one local edit: syntax `p95 <= 10ms`
2. 5k-line file, two distant edits in one batch: syntax `p95 <= 15ms`
3. No crash or invalid tree on malformed intermediate edit states

---

## Completion Criteria

M11 is complete when:

1. T11.01–T11.05 pass
2. Current IDE/LSP regression tests remain green
3. The syntax layer consumes structured multi-edit deltas when available
4. The full-text fallback path still works for non-incremental clients
