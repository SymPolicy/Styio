# Review Docs

**Purpose:** Define the minimal current-tree review policy: keep only still-active review findings, move durable rules into owning SSOTs, and recover old review prose from Git history.

**Last updated:** 2026-07-25

## Scope

1. Store unresolved review findings here only while they need direct follow-up.
2. Fold reusable lessons into `../rollups/` and owning active SSOTs.
3. Keep language SSOT in `docs/design/`.
4. Do not use this directory as a parallel syntax, design, or checkpoint-history SSOT.
5. Resolve tradeoff disputes under [../specs/PRINCIPLES-AND-OBJECTIVES.md](../specs/PRINCIPLES-AND-OBJECTIVES.md).

## Naming Rules

1. Every active review document other than `README.md` and the generated
   `INDEX.md` contains unresolved work and must end in `-Draft.md`.
2. A document anywhere in the repository that still awaits an owner decision
   must declare `**Status:** Draft` near its title and end in `-Draft.md`.
3. `Pending implementation` does not mean `Draft`: an implementation plan for
   an accepted design keeps its ordinary descriptive filename.
4. When the final owner decision closes, promote the accepted rule into its
   owning SSOT and remove the review draft. If the same file becomes the owning
   SSOT, remove `-Draft` in that same change; do not leave a compatibility file
   at the old path.
5. Use Git history when exact old review wording is required.

## Inventory

See [INDEX.md](./INDEX.md).
