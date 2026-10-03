# Release Policy

The `nightly` branch is a development lane. It supports source builds and local
verification, but it does not imply a packaged release channel.

Release artifacts require:

- a tagged commit
- recorded build inputs
- dependency and license review
- source and binary checksums
- current-candidate release admission, retained stable evidence and distribution-check output
- GitHub Actions evidence for the tagged commit

Do not describe an artifact as released until those records exist in the
repository or in the release entry.


## Internal Promotion

There is one public compiler update stream, `release`. Internal nightly/stable/
release branches progressively accumulate validation; they are not three public
versions. Stable executes only missing compatible scope on Windows, Linux and
macOS. Release consumes their exact sealed stable payloads and adds distribution
checks, without rebuilding or rerunning inherited regression scope.

See [Incremental Promotion](docs/specs/INCREMENTAL-PROMOTION.md) for producer trust,
current-candidate admission, applicability reports, metadata compatibility and
rollback. A candidate artifact is not a published release. This CI workflow has
no permission to publish releases or modify protected-branch rules.
