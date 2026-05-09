# Change Bootstrap Env

**Purpose:** Change environment dependency bootstrap behavior without turning bootstrap into build or delivery automation.

**Last updated:** 2026-04-23

**TOML:** [CHANGE-BOOTSTRAP-ENV.toml](./CHANGE-BOOTSTRAP-ENV.toml) is the machine-readable workflow definition.

## Skill

Use [styio-env-bootstrap/skill.toml](./skills/styio-env-bootstrap/skill.toml) when changing `scripts/bootstrap-dev-env.sh` or the dev environment contract.

## Workflow

1. State whether the change is a dependency, version baseline, path, or documentation update.
2. Keep bootstrap limited to dependency preparation.
3. Update `docs/BUILD-AND-DEV-ENV.md` and any standard baseline text.
4. Add or update a non-invasive test that does not install packages.
5. Run syntax, docs, and workflow gates.

## Required Evidence

1. Dependency or baseline changed.
2. Bootstrap scope remains dependency-only.
3. Help text and docs agree.
4. Test avoids mutating the host package state.

## Gates

```bash
bash -n scripts/bootstrap-dev-env.sh
ctest --test-dir build/default -R '^bootstrap_dev_env_dependency_scope$' --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
git diff --check
```

## Handoff

Report changed dependencies, host assumptions, exports, tests, and anything not installed by bootstrap.
