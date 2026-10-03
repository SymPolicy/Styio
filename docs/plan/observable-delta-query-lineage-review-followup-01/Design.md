# IDE cache directory failure repair

## Boundary

The IDE index cache is an optional persistence layer. Its directory and file
writes may fail in read-only or sandboxed cache roots, but those failures must
not terminate `styio_lspd`, change cache identity, or change LSP stdio bytes.

## Repair

Use `std::error_code` for cache-directory creation and treat an unsuccessful
`symbols.json` open as a cache miss. Keep the in-memory index and protocol
request handling active when persistence is unavailable.

## Evidence

PR #42 adds invalid-cache persistence coverage to the IDE unit suite and a
real stdio initialize smoke under an unavailable cache root. The merged
nightly commit passed Linux, macOS, smoke, golden-standard, hygiene, audit,
and Windows smoke checks.
