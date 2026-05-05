# Resource Identifier Surface

**Purpose:** Map resource identifier changes to syntax, semantic, runtime, and docs surfaces.

**Last updated:** 2026-05-04

Resource changes start from internal Styio source:

- Add or update `src/StyioPrelude/resources.styio`.
- Use `@ name : type := #(args) => { body }` for resources with a declared type.
- Use `@ name := #(args) => { body }` only for fixed standard-resource declarations where the
  type is already fixed by the resource family.

Parameterized resource expressions are not declaration heads.
Invalid example: `@name{arg} := { ... }` as a resource definition form.
Hidden pseudo-primitives such as `file(path)` are not allowed; C++ may only provide substrate
behavior once the resource identity is defined in Styio.

| Surface | Examples |
|---------|----------|
| syntax SSOT | `docs/design/syntax/RESOURCE_IDENTIFIERS.md` |
| prelude source | `src/StyioPrelude/resources.styio` |
| lifecycle docs | resource ownership, close, transfer, error behavior |
| lexer/parser | `@name`, resource literals, write/read shorthand |
| analyzer | copy rules, ownership, type family, fail-closed diagnostics |
| runtime | handle acquisition, release, invalid handle behavior |
| tests | lexer/parser/security tests and runtime smoke |
