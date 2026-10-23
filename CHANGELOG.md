# Changelog

This project does not currently publish tagged release notes from the nightly
branch. Until the release process is active, use Git history and repository
documentation for change provenance.

## Unreleased

- 2026-05-22 history-burden reduction pass:
  - Retired the `docs/assets/workflow/` mirror; root `workflows/` is now the
    single canonical location for reusable workflow documents and skills.
  - Removed dead source shells (`src/StyioAnalyzer/`,
    `src/StyioJIT/JITExecutor.hpp`, `src/StyioIR/DFIR/`, four zero-byte `.cpp`
    placeholders, the unused `src/cmake/StyioIDESources.cmake`).
  - Merged the three `SIO*` IO node classes from the former
    `src/StyioIR/IOIR/IOIR.hpp` into `src/StyioIR/GenIR/SIOIR.hpp`.
  - Folded the 57-line `src/StyioCodeGen/CodeGen.cpp` driver shard into
    `src/StyioCodeGen/CodeGenG.cpp`.
  - Moved `FindICU.cmake` from the repository root into `cmake/`.
  - Removed the retired `tests/milestones/m11/` fixture set and stray local
    build directories (~1.25 GB).
  - Recorded every still-open historical compatibility seam in
    [`docs/rollups/MIGRATION-LEDGER.md`](docs/rollups/MIGRATION-LEDGER.md);
    each open site carries an inline `MIGRATION-NEEDED:` marker pointing at
    its ledger row and closure signal.
- Resource topology parsing and typed stdin tuple ingestion are active on the
  nightly branch.
- Public examples are limited to runnable files covered by local tests.
