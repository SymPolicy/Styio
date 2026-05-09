# Correct Syntax Contract

**Purpose:** Record the reusable workflow for correcting a Styio syntax contract when compiler behavior, tests, and language specification disagree about accepted or rejected spelling.

**Last updated:** 2026-05-01

## Canonical Workflow

The root workflow is [../../../workflows/CORRECT-SYNTAX-CONTRACT.md](../../../workflows/CORRECT-SYNTAX-CONTRACT.md), with the machine-readable definition in [../../../workflows/CORRECT-SYNTAX-CONTRACT.toml](../../../workflows/CORRECT-SYNTAX-CONTRACT.toml).

Agents must read that workflow before changing parser, Sema, lowering, tests, or syntax docs for a disputed spelling.

## Trigger

Use this workflow when a user or maintainer reports that:

1. a syntax form parses but should be rejected;
2. a syntax form should parse but falls into the wrong parser or type error;
3. EBNF, design docs, examples, and compiler behavior disagree;
4. the answer needs the exact compiler diagnostic, not only a design explanation.

## Required Closure

1. Reproduce the disputed spelling with the smallest `.styio` program.
2. Identify whether rejection belongs to parser, Sema/type inference, lowering, runtime, or docs.
3. Freeze the accepted spelling and rejected spelling before editing.
4. Update implementation while preserving the existing visitor architecture unless a new AST node is required.
5. Add positive and negative tests for the accepted and rejected forms.
6. Update EBNF, language design, symbol reference, runbooks, test catalog, and generated indexes as relevant.
7. Re-run the minimal sample and report the exact diagnostic.
8. Run focused tests, docs gates, and `git diff --check`.

## Handoff

Report the rejected spelling, accepted spelling, owning grammar rule, exact diagnostic, standard-doc changes, gates run, and any remaining parser-route, IDE grammar, or lowering work.
