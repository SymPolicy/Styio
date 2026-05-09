# File Onboarding Surface

**Purpose:** Map new file classes to the repo surfaces that usually need updates.

**Last updated:** 2026-04-23

| Class | Usual Surfaces |
|-------|----------------|
| doc | `README.md`, `INDEX.md`, `scripts/docs_config.py` for new collections |
| workflow | `workflows/*.toml`, `workflows/workflows.toml`, workflow docs |
| skill | `workflows/skills/*/skill.toml`, `workflows/workflows.toml` |
| script | shell/python syntax check, docs gate references |
| source | target build, focused unit/security test |
| test | `tests/CMakeLists.txt` or matching test registry |
| config | config docs, parser/consumer smoke |
| fixture | repo hygiene allowlist or negate rule if needed |
