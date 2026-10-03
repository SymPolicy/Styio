# Incremental Promotion and One Public Update Stream

**Purpose:** Define executable nightly-to-stable-to-release admission, trustworthy evidence reuse, three-platform acceptance, and report-only discovery boundaries.

**Last updated:** 2026-10-03

## Public Product and Internal Stages

There is one public compiler update stream, `release`. The internal branches
`nightly`, `stable`, and `release` are integration, validation, and promotion
stages, not three public product versions. An internal build does not constitute
publication. This workflow creates candidate artifacts only: it does not create
a tag, GitHub Release, deployment, credential, or protection-rule change.

`--machine-info=json` retains `channel` for compatibility, now identifying the
single public update stream. It also exposes `public_update_channel` and
`build_id`; `compiler_version` and `variant` keep their meanings. Full and nano
are variants in that stream. A normal local build reports `local-unsealed` as
its build identity. CI supplies a source/toolchain/configuration identity that
is independent of a promotion commit SHA. Package-specific `--nano-channel`
options remain package registry settings, not public compiler update streams.

The source-build-info contract retains legacy nightly/stable source references
for developer tools and labels them `public_update=false`; release is the public
source reference. Existing fields remain present. Consumers must not infer
publication from a binary's update-stream name: publication state is external.

## Responsibilities and Triggers

The implementation is `.github/workflows/styio-ci-gate.yml`,
`configs/promotion-ci.json`, and `scripts/promotion-ci.py`.

- Ordinary nightly: one Linux Release build, the compact configured smoke set,
  documentation/process checks, and the security floor. Changes to native
  adapters or platform recipes add the same basic scope on affected official
  platforms; they do not add stable's full regression to ordinary nightly.
- Stable: compute its required scope on Windows, Linux and macOS together, reuse
  compatible evidence, build only missing targets/configurations, and execute
  only missing actual test names and applicable example specifications.
- Release: choose one compatible successful stable producer for all three
  platforms; download its exact sealed payloads, verify integrity and installed
  identity/execution, and record release admission. No compiler rebuild or
  inherited business-test rerun is permitted at release.

PRs into stable must come from this repository's nightly branch; PRs into release
must come from this repository's stable branch. A protected-stage push must be
associated with its merged predecessor-branch PR. Nightly remains the normal
entry for task branches. The existing seven-check `ci-prebuild` policy chain
remains in the Linux routing job, with the established `styio-nightly`,
`styio-pafio`, and `styio-view` sibling layout. It verifies current-change
hygiene, runtime boundaries, team docs and cross-repository docs without
compiling or rerunning business tests; delta-sensitive policy checks are
executed for the current candidate rather than treated as old test evidence. Policy rollout starts with a reviewed nightly PR;
stable/release acquire the implementation through authorized promotions, never
through direct protected-branch writes or silent replacement of their trees.

## The Missing-Scope Calculation

Required scope minus compatible successful evidence equals work to execute.
The unit of test evidence is the actual CTest name and command/properties, not
an overlapping label. A test selected by security and another suite still runs
once. Stable's scope is the configured catalog minus explicitly reasoned
scheduled-only labels, plus discovered applicable examples. A zero match for a
required nightly selector, missing executable, skipped test, cancellation or
unknown result cannot satisfy required acceptance.

Source identity uses existing Git blob/tree object IDs for relevant paths. A
small canonical manifest digest identifies inputs; the implementation does not
read and hash the whole repository to decide whether a test should run. Build
configuration, compiler/LLVM/CMake/Ninja versions, SDK, runner environment,
platform/architecture and declared dependency pins are part of identity.
Cross-platform evidence is never interchangeable. Documentation checks include
their documentation inputs; unrelated documentation does not change compiler
build inputs. Test-context invalidation is intentionally conservative within the
configured tests/fixtures/runner scope: narrowing it later requires dependency
proof, not assumptions.

A new merge SHA alone does not invalidate unchanged relevant inputs. Changed
source, generated-input recipes, tests/fixtures, selectors, policy, toolchains,
dependencies or relevant environment invalidate affected evidence. Unknown or
unavailable provenance means missing scope. A later non-successful protected
producer is not hidden by searching only successful historical runs. Failure
invalidation is scoped to its actual platform job, build identity and test
identity. A platform absent from a Linux-only matrix cannot invalidate Windows
or macOS evidence. Missing failure details conservatively require that platform's
older scope to be revalidated; malformed test results never preserve a pass.

## Evidence Trust and Build Reuse

Only independently verified GitHub Actions run/artifact/check metadata grants
reuse. The repository and workflow path must match policy; checks and the run
must have succeeded, artifacts must be retained and bound to that run/attempt,
and the source must be a protected permitted branch. A same-repository PR run
may be reused after the PR actually merged into an allowed protected target;
forks, unmerged PRs, mismatched heads/targets and manifest self-attestations are
not trusted. This permits landing reconciliation to reuse the admitted PR's
unchanged work. Current-candidate admission still records the current checkout,
source head/base, producer run/check/artifact, reused scope and newly run scope.

Artifacts are retrieved by immutable artifact ID, not an ambiguous name or an
unqualified latest-success badge. Payload checksums protect sealed distribution
bytes. Failed/partial evidence never becomes reusable success. Missing or expired
build caches cause missing build work; missing stable payload/evidence returns a
release candidate to stable instead of silently rebuilding it at release.

A compatible CMake/Ninja build tree may be restored on the same path/toolchain
profile. Ephemeral checkout timestamps are normalized after identity validation
so an unchanged promotion checkout does not cause a rebuild solely due to file
mtime. An incompatible generated build cache is discarded before normalization;
source files, user branches and other worktrees are never deleted. Rebuildable
build caches are retained for seven days; smaller verification evidence and
sealed stable/release payloads retain the 90-day window. An expired build cache
requires missing build work, not automatically repeating still-valid tests. Build reuse
and test reuse are separate: a cached binary does not imply its tests passed.

## Examples and Advisory Findings

`scripts/promotion_examples.py` discovers tracked examples automatically, including
staged additions, with no per-file inclusion manifest. Existing CTest command
references identify consumers; a shell wrapper does not automatically prove its
same-named Styio source executed. Convention-compatible stdout golden examples
receive automatic specifications with source/stdin/output identity paths.
Missing input, external requirements, ambiguous fixtures, wrappers without an
invocation or unknown applicability produce explicit advisory findings.

Discovered, registered, executed and passed are distinct states. Unknown new-file
coverage and unknown example applicability are report-only; they do not create a
commit, CI, or merge blocker. Real failures of applicable required checks remain
blocking. In this upstream baseline the catalog covers three Styio source files
and the calculator shell; the independent calculator source is an additional
automatically discovered golden. No result is reported as passing before execution.

## Platforms and Distribution

Stable and release require Windows, Linux and macOS together; tolerated Windows
failures cannot be reported as acceptance. LLVM 18.1.x and exact compiler/config
fingerprints are recorded. LibFuzzer is supported in the Linux/macOS Clang
configuration. The MSVC configuration records why libFuzzer is unavailable;
Windows runtime/build/example acceptance is still mandatory. Scheduled long
fuzz/soak campaigns and explicitly requested performance reports remain separately
identified scopes. There is no assertion that opt-in performance reports run in
the existing scheduled workflows.

Stable installs and seals the Runtime component, product identity, licenses and
runtime prerequisites. The candidate currently expects native LLVM 18.1 and its
platform runtime dependencies; the native compiler toolchain is not bundled.
The report must not describe this as a dependency-free standalone package.
Release verifies the same payload bytes, safe extraction, product/version/build
identity, licensing presence, installed execution and temporary uninstall. These
are new distribution checks, not a repetition of stable regression tests.
Release also probes the current native runtime/toolchain environment without
configuring or building; an incompatible environment returns the candidate to
stable. Distribution metadata inputs have their own identity, so license or
packaging-document edits cannot silently reuse outdated sealed contents.

Windows test fixtures explicitly use the runner's verified Git for Windows Bash,
while production CLI process launches use LLVM argument-vector APIs. Windows
native temporary fixtures use the current drive's temporary directory; generated
nano subset builds use Ninja. These are native-platform compatibility adapters,
not a change to language semantics.

One stable run/attempt must supply the complete three-platform payload set.
Rollback selects a previously admitted immutable set and its provenance; it does
not reset shared branches or rebuild an old source tree and call it the old
artifact. Publication, version/tag decisions and actual rollback remain explicit
maintainer actions outside this implementation.

## Governance and Verification

The existing required check name `styio-ci-gate` remains the admission authority,
and `styio-audit` remains required. The workflow cannot alter Rulesets. Any exact
protection change, including latest-base strictness, requires separate approval
and is applied only after the new flow is verified. Reports do not bypass checks.

Each platform writes machine-readable evidence and a human-readable report from
already executed results. The final job collects those records into
`admission.json` and `admission.md` and a GitHub job summary, including actual
check statuses, reused source references, example findings and separately
scheduled scopes. Reporting never runs tests again; diagnostic report upload
failure is advisory, while real admission failures remain required failures.

Focused regression commands:

```bash
python3 -m unittest discover -s tests -p 'promotion_*_test.py'
STYIO_TEST_NATIVE_COMPILER=clang++ python3 tests/promotion_platform_test.py
```

Fixtures cover SHA-only reuse, invalidation, trusted provenance, failed/skipped
and expired evidence, test-name deduplication, examples, archive safety, malformed
results and all-platform current-candidate admission. Native Windows/macOS CI
and actual artifact lookup/restore must be observed before reporting deployment
acceptance. Offline fixtures and a Linux build do not substitute for those runs.
