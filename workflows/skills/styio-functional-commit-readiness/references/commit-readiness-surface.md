# Commit Readiness Surface

## Surface Inventory

1. Feature entrypoints: function, class, CLI flag, service endpoint, parser route, runtime helper, workflow profile, or docs command changed by the work.
2. Upstream producers: configs, generated files, source fixtures, compile plans, source-build metadata, parser/tokenizer input, package data, environment variables, and docs/runbook instructions that feed the feature.
3. Downstream consumers: tests, CTest labels, CLIs, IDE/LSP paths, nano/spio handoff pages, runtime/codegen callers, examples, benchmark probes, generated indexes, and owner runbooks.
4. Cutover surface: old names, routes, adapters, fixtures, docs wording, compatibility paths, and fallback behavior when the feature replaces or migrates behavior.

## Minimum Evidence Ladder

1. A targeted feature command proves the changed behavior directly.
2. An upstream check proves the feature receives the expected input shape.
3. A downstream check proves at least one real caller consumes the new behavior.
4. The owner-team gate proves the affected subsystem stays aligned.
5. The delivery or checkpoint gate proves repository-wide process health for the chosen scope.

## Unable-To-Verify Record

Record objective blockers with this shape:

```text
Unverified surface: <path, platform, service, command, or consumer>
Skipped command: <exact command or "none available locally">
Blocker: <objective reason>
Unblocker: <person, system, credential, platform, sibling repo, or CI job>
Substitute evidence: <command/result that was run instead>
Follow-up gate: <command or CI check that must pass later>
```

Use this only for external or environment constraints. Local uncertainty, missing targeted tests, or incomplete caller adaptation is not an unable-to-verify blocker; continue implementation instead.
