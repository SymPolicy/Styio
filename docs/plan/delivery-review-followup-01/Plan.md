# PLAN-002 Align malformed-source IDE semantic publication contract

Phase: draft · Revision: unsealed

This document is a render-only projection of `Plan.json`. Edit `Plan.json`; never edit this file.

## Intent

**Goal**: The compiler-backed IDE bridge currently invokes the Nightly parser in recovery mode and publishes later semantic facts after malformed input, while several active inventories, IDE documents, and workflow selectors require strict rejection and reference a nonexistent rejection test.

**In scope**
- The compiler-backed IDE bridge currently invokes the Nightly parser in recovery mode and publishes later semantic facts after malformed input, while several active inventories, IDE documents, and workflow selectors require strict rejection and reference a nonexistent rejection test.
- src/StyioServices/StyioIDE/CompilerBridge.cpp
- tests/ide/styio_ide_test.cpp
- docs/rollups/IM-D2-PARSER-AUTHORITY-INVENTORY.md
- docs/rollups/IM-D9-IDE-LSP-SERVICE-CONTRACT-INVENTORY.md
- docs/external/for-ide/BUILD.md
- docs/external/for-ide/CXX-API.md
- docs/external/for-ide/LSP.md
- docs/external/for-ide/TESTING.md
- docs/teams/IDE-LSP-RUNBOOK.md
- workflows/TEST-CATALOG.md
- workflows/PROMOTE-NIGHTLY-PARSER-SUBSET.md

**Out of scope**
- Changes unrelated to PLAN-002's confirmed repair outcome.

**Success**
- One owner-approved malformed-source semantic-publication policy is implemented consistently.
- Compiler behavior, IDE/LSP documentation, inventories, and registered CTest selectors agree.
- Every documented focused selector registers at least one passing test.

**Risk boundary**
- Keep the Nightly compiler parser authoritative for accepted syntax.
- Do not treat recovery as successful compilation or accepted malformed syntax.
- Preserve diagnostics, snapshot freshness, and cancellation behavior.
- Do not break the public SemanticSummary ABI without an explicit migration.

## Decisions

Dossier status: not_required

No non-discoverable user decision was required.

### Observed repository facts

- The compiler-backed IDE bridge currently invokes the Nightly parser in recovery mode and publishes later semantic facts after malformed input, while several active inventories, IDE documents, and workflow selectors require strict rejection and reference a nonexistent rejection test. (source: src/StyioServices/StyioIDE/CompilerBridge.cpp)
- Maintainers and IDE consumers cannot determine whether recovered semantic facts are contractual. Documented CTest commands can select zero tests, and future changes may silently alternate between useful recovery and strict fail-closed behavior. (source: src/StyioServices/StyioIDE/CompilerBridge.cpp)
- The registered StyioSemanticBridge.RecoversNightlyParseForLaterStatements test passes, while StyioSemanticBridge.RejectsMalformedInputWithoutRecovery selects zero tests. CompilerBridge.cpp explicitly uses StyioParseMode::Recovery; active documentation contains both strict-rejection and recovery contracts. (source: src/StyioServices/StyioIDE/CompilerBridge.cpp)
- Choosing strict semantic rejection versus recovered IDE facts changes the IDE semantic-publication contract. That decision is outside PLAN-001's Q1-Q10 repairs and its prohibition on unapproved semantic expansion. (source: delivery/Plan.json)

## Requirements

None recorded yet.

## Architecture



- none

## Tasks

None recorded yet.

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- `none`
- paths: none
