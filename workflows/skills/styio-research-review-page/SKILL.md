---
name: styio-research-review-page
description: Build and maintain portable HTML review pages for Styio repositories, with versioned source facts, per-repository proposals, Draft PR status, and verification evidence. Use for maintainer-facing engineering and research reviews, not the separate skill for authoring Styio programs.
---

# Styio Research Review Page

**Purpose:** Produce a readable, reproducible review surface from current repository evidence and explicitly separate proposals.

**Last updated:** 2026-10-01

## Use The Repository As The Source

Read [the docs maintenance workflow](../../DOCS-MAINTENANCE-WORKFLOW.md), the
affected owner documents, and the selected repository's contribution rules.
Keep each repository's plan, implemented changes, benefit and cost, verification,
and pending decisions together. Use a direct Draft PR link for reviewable remote
changes; record local work, pushed candidates, exact-head CI, and merged changes
as separate states. Refresh the evidence after a new commit. A successful local
build or an older CI run does not establish the new candidate's status.

The page is a projection of repository facts and review decisions. Keep semantic
definitions in the existing feature/grammar contracts and operational commands
in their owners. Link the governing source revision; do not copy a second
maintained specification into the page input.

## Build A Portable Page

Use Python 3.10 or later and Git. The bundled generator uses the standard library
and local CSS. It has no hosting-provider identity, account, or deployment path.
Read [the input schema](references/input-schema.md), then adapt
`fixtures/site.json` into an input directory. Its examples are synthetic.

The repository list drives the first navigation level. Each repository has the
same review sections, while user documentation, programming skills, and formal
research retain separate navigation groups. Structured text/code/table blocks
avoid Markdown pipe ambiguity. Pre-rendered, self-contained mathematical HTML
can be imported without turning formulas into raw TeX in a code block.

```bash
python3 workflows/skills/styio-research-review-page/scripts/build_review.py \
  --input <review-input>/site.json --output <review-output>
python3 -m unittest discover \
  -s workflows/skills/styio-research-review-page/tests
python3 workflows/skills/styio-research-review-page/scripts/test_extract_facts.py
```

Open the generated `index.html` in a browser and inspect repository navigation,
long tables, code, mathematical notation, narrow screens, and direct section
links. The generator checks output links and state invariants; it does not claim
to have performed visual inspection or verified external CI by rendering a URL.
The output is readable from local files. Use an authorized hosting destination
when the reviewer needs a stable URL; building the page does not publish it.

## Refresh Committed Facts

The extractor reads immutable Git blobs at the resolved commit, excluding dirty
and untracked files and local Git replacement objects. It exports composed EBNF,
the active authoring map, the document tree, source directories, the declared
layer-checker contract, and static CMake target/link declarations.

```bash
python3 workflows/skills/styio-research-review-page/scripts/extract_facts.py \
  --repo <compiler-checkout> \
  --repository-url https://github.com/<owner>/<repository> \
  --ref refs/heads/nightly --classification current --fetch \
  --output <review-input>/facts.json \
  --status-output <review-input>/facts-status.json
```

For a Draft PR, select its exact fetched head commit and `--classification draft`.
Omit `--fetch` when intentionally using an already available local Git ref. The
output records the resolution mode, ref, commit, and UTC extraction time. The
classification is supplied by the caller; verify the intended branch/PR before
labeling the view. The extractor's Styio authority paths are deliberate: a
repository without those owners must not be presented as a complete Styio fact
snapshot.

EBNF is the composed grammar, not proof that every construct is implemented.
Feature lifecycle stays with `docs/design/syntax/features/`. CMake declarations
include optional branches, aliases, and unresolved variables. They are not an
evaluated build graph, a complete dependency graph, or runtime dataflow. Preserve
these limitations beside the generated view.

Always provide both fact data and status to the renderer. A failed refresh keeps
the last successful data and marks it stale; an initial failure is unavailable.
Do not relabel the preserved commit as the new requested ref. Schedule the same
explicit refresh/build commands when daily or hook-driven maintenance is wanted.
Render failure status even when extraction returns nonzero, then return failure
to the scheduler. Serialize refreshes that share output paths.

The reusable daily/hook entrypoint performs that sequencing:

```bash
python3 workflows/skills/styio-research-review-page/scripts/refresh_review.py \
  --config <review-input>/refresh.json
```

Adapt `fixtures/refresh.json` to the intended checkout and source ref. It accepts
multiple explicit current/draft sources, renders stale or unavailable results
after an extraction failure, and returns nonzero for the scheduler. It does not
install a schedule, query CI, upload files, or publish the generated site.

Treat extraction, building, uploading, and publication as separate operations.
A repository hook in a Draft PR is a proposed integration until that workflow
has landed and a real run has succeeded. Do not change repository permissions,
enable publication, or install a schedule solely because this skill is invoked.

## Complete The Review

Run the generator and extractor tests, registry check, documentation audit,
generated-index check, and applicable runbook gates. Report the actual source
and PR head, tests passed/failed/not run, visual inspection scope, refresh time,
and any stale facts. Record research hypotheses and pending choices in the plan
sections, without presenting them as completed compiler capabilities.
