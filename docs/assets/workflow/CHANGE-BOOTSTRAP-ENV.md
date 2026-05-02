# Change Bootstrap Env

**Purpose:** Mirror the root workflow for changing environment dependency bootstrap behavior.

**Last updated:** 2026-04-23

Canonical workflow: [../../../workflows/CHANGE-BOOTSTRAP-ENV.md](../../../workflows/CHANGE-BOOTSTRAP-ENV.md)

## Workflow

1. Classify the change as dependency, version baseline, path, or documentation.
2. Keep bootstrap limited to dependency preparation.
3. Update `docs/BUILD-AND-DEV-ENV.md`.
4. Add or update a non-invasive test.
5. Run syntax, docs, and workflow gates.

## Gates

```bash
bash -n scripts/bootstrap-dev-env.sh
ctest --test-dir build/default -R '^bootstrap_dev_env_dependency_scope$' --output-on-failure
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
git diff --check
```
