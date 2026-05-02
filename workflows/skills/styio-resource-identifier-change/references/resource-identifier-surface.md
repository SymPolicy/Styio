# Resource Identifier Surface

**Purpose:** Map resource identifier changes to syntax, semantic, runtime, and docs surfaces.

**Last updated:** 2026-04-23

| Surface | Examples |
|---------|----------|
| syntax SSOT | `docs/design/syntax/RESOURCE_IDENTIFIERS.md` |
| lifecycle docs | resource ownership, close, transfer, error behavior |
| lexer/parser | `@name`, resource literals, write/read shorthand |
| analyzer | copy rules, ownership, type family, fail-closed diagnostics |
| runtime | handle acquisition, release, invalid handle behavior |
| tests | lexer/parser/security tests and runtime smoke |
