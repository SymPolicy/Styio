# Local Info Leak Gate

**Purpose:** Prevent repo-local skills, docs, workflows, scripts, tests, and handoff records from exposing developer-machine paths, server filesystem paths, private endpoints, or host-specific deployment details.

**Last updated:** 2026-06-28

**TOML:** [LOCAL-INFO-LEAK-GATE.toml](./LOCAL-INFO-LEAK-GATE.toml) is the machine-readable workflow definition.

## Goal

Repository-owned files must use placeholders for local and server-specific information. A useful instruction can say `<workspace-root>`, `<user-home>`, `<server-root>`, `<server-host>`, `<service-url>`, `<private-ip>`, or an environment variable such as `$STYIO_ECOSYSTEM_WORKSPACE`; it must not record a real maintainer path, account name, host path, private IP, SSH target, or deployment root.

This rule is stricter for repo-local skills under [skills/](./skills/): skills are reusable agent instructions, so they must never carry developer-machine or server-machine structure. If a skill needs an example, write it with placeholders.

## Command

Worktree scan:

```bash
python3 scripts/local-info-leak-gate.py --mode worktree
```

Tracked tree scan:

```bash
python3 scripts/local-info-leak-gate.py --mode tracked
```

Staged scan:

```bash
python3 scripts/local-info-leak-gate.py --mode staged
```

Push-range scan:

```bash
python3 scripts/local-info-leak-gate.py --mode push --range <base>..<head>
```

## What It Warns On

The gate prints `WARNING` findings and fails unless explicitly run with `--warning-only`. Current high-confidence findings include:

1. Windows absolute paths such as drive-rooted local paths.
2. POSIX user paths such as home-directory paths.
3. Server filesystem roots such as deployment directories.
4. SSH-style user/host targets.
5. Non-example IPv4 addresses.
6. UNC server-share paths.

Use TEST-NET documentation addresses when an IP example is required, and use placeholder path components for parser/security fixtures, such as `C:/<drive-root>/file` or `\\\\<server>\\<share>\\file`.

## Scan Semantics

- `--mode push` and `--mode staged` scan the exact committed/indexed blobs (`git show`), not the live worktree, so the reviewed bytes are the bytes that leave the machine.
- The gate fails closed: files whose content cannot be read, and binary files outside the documented exclusion, are reported as `UNREVIEWED` and fail the run.
- `tests/fuzz/corpus/` is the narrow binary exclusion: fuzz seeds are raw byte inputs, not publishable text.
- A line containing the literal `local-info: allow` is exempt from all rules. Use it sparingly for generated or exceptional lines; the exemption applies to that line only.
- Exception values (documentation networks, loopback literals, the public `git@github.com:` SSH endpoint used by delivery/test fixtures) live in `scripts/local-info-leak-gate.policy.json`, which the gate validates at startup and excludes from its own scan set.

## Required Evidence

1. Scan mode and command.
2. Result, including every `WARNING` if any.
3. Replacement placeholders used for cleaned findings.
4. Confirmation that repo-local skills still contain no real local or server structure.

## Failure Modes

Stop delivery when any of these remain:

1. A skill contains a real developer path, server path, host, account name, or private endpoint.
2. A doc or workflow records a maintainer-specific checkout path instead of `<workspace-root>` or an environment variable.
3. A script has a hard-coded local fallback path that should be configured through an environment variable.
4. A test fixture uses a realistic private endpoint when a TEST-NET or placeholder value would prove the same behavior.
