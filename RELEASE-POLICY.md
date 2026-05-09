# Release Policy

The `nightly` branch is a development lane. It supports source builds and local
verification, but it does not imply a packaged release channel.

Release artifacts require:

- a tagged commit
- recorded build inputs
- dependency and license review
- source and binary checksums
- local gate output for the release profile
- GitHub Actions evidence for the tagged commit

Do not describe an artifact as released until those records exist in the
repository or in the release entry.
