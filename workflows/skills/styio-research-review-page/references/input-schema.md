# Input contract (schema_version 1)

Purpose: Specify portable repository-review inputs and validation boundaries.

Last updated: 2026-10-01.

`review.json` is UTF-8 JSON. Paths are relative to this input file and must stay within its directory; output paths are explicitly supplied to the CLI. See the executable `fixtures/site.json` for all block forms and a complete model.

Top-level fields:

- `schema_version`: exactly `1`
- `title`: project name
- `updated_at`: explicit ISO-8601 observation time; do not imply a failed refresh succeeded
- `repositories`: repository entries
- `pages`: user-facing documents or self-contained papers
- `navigation` (optional): group -> target HTML route or verified HTTPS URL; overrides the first entry in each group

Groups: `engineering`, `user-docs`, `user-skill`, `research`. They render as fixed Engineering/User docs/User skill/Formal research navigation, with the current group marked active. Mobile navigation wraps into two columns; anchor scroll margins reserve header space.

## Repository entry

Required fields: `id`, `name`, `group`, `view` (`current` or `draft`), `ref`, `commit`, `state`, `sections`. Optional `url`, `route` (defaults to `repositories/<id>/index.html`), `facts`.

`state` contains:

- `local`: `none`, `in_progress`, `complete`
- `publication`: `local_only`, `pushed`, `draft_pr`, `merged`, `paused`
- `remote_commit`: required for pushed/Draft/merged and must equal the selected full 40-hex commit
- `ci`: `not_run`, `pending`, `passed`, `failed`, `not_applicable`
- `pr`: when applicable, `{url, state, head_commit}`. Draft publication requires state `draft`; merged publication requires state `merged`; head must match the selected commit
- `ci_evidence`: required for passed CI, `{url, head_commit, scope, checked_at}`. Its head must match the selected commit and the scope must identify what actually ran

`sections` requires `plan`, `implemented`, `value`, `verification`, `pending`, each an array of blocks. `value` records original problem, actual change, evidenced benefit/cost and remaining uncertainty. Narrative text still needs human review: structural validation cannot prove a prose claim true.

## Blocks

- `{type: "paragraph", text: "..."}`
- `{type: "list", items: ["..."]}`
- `{type: "code", text: "..."}`
- `{type: "link", href: "guide/index.html#topic", label: "..."}` or a verified HTTPS source
- `{type: "table", headers: ["..."], rows: [["..."]]}`; every row must have the same number of cells

Text is HTML-escaped. Generated internal links use relative filenames and remain usable without an HTTP server. Unknown block types, traversal routes and broken links/anchors fail the build.

## Standalone pages

Required `title`, `route`, `group`. Supply exactly one of:

- `blocks`: normal content
- `html_file`: a complete, self-contained HTML paper
- `facts`: `{data: "facts.json", status: "facts-status.json"}`

## Fact data

The collector emits `source{repo_url,ref,commit,view,generated_at}`, `grammar[{path,text,sha256,role}]`, `documents[{path}]`, `cmake{targets,links,files,limitations}`, and `code{directories,boundary_contract}`. CMake links can have `unresolved:true` for variable/generator-expression targets. The separate status has `state: fresh|stale|unavailable`, `attempted_at`, `last_success`, `commit`, and a generic `error` code. Fresh and stale statuses must match the fact commit; a mismatch renders an unavailable integrity-error panel without displaying the mismatched data. Failure must not overwrite last-good facts or imply freshness.

The first-run unavailable case has no data to render. The status-only unavailable panel is supported. Preserve its explicit error and do not publish fabricated facts; retry only through an authorized path.

## Refresh configuration

`scripts/refresh_review.py --config refresh.json` accepts a separate JSON object
with `schema_version: 1`, `review_input`, `output`, and a nonempty `sources` list.
Paths resolve from the configuration directory. Each source specifies `repo`,
`repository_url`, `ref`, `classification`, optional boolean `fetch` (default
false), `facts_output`, and `status_output`. Fact/status destinations must be
distinct JSON files inside the review-input directory; they cannot overwrite
the input model or refresh configuration. See `fixtures/refresh.json`.

The entrypoint processes sources serially, then renders even if an extraction
failed. Exit status is 0 for successful extraction and rendering, 1 when stale
or unavailable extraction results were rendered, and 2 when configuration or
rendering failed. The renderer preserves its previous complete output if a new
build fails. Serialize separate scheduled invocations that share output paths.
No schedule, hook, remote push, upload, or publication is installed by this tool.

## Deliberate limits

The generator does not infer publication, PR, CI or merge outcomes; supply verified service evidence. It does not run project build scripts or query a hosting API. It cannot certify visual layout, semantic correctness, access permissions, user-study outcomes or formal proof completion. CMake extraction is conservative static declaration extraction, not configured dependency evaluation.
