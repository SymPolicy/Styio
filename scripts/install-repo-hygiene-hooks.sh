#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOOK_DIR="$ROOT/.git/hooks"

if [[ ! -d "$HOOK_DIR" ]]; then
  echo "[repo-hygiene] missing .git/hooks at $HOOK_DIR" >&2
  exit 1
fi

mkdir -p "$HOOK_DIR"

cat >"$HOOK_DIR/pre-commit" <<'HOOK'
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
exec "$ROOT/scripts/delivery-gate.sh" --mode staged --skip-health --skip-audit
HOOK

cat >"$HOOK_DIR/pre-push" <<'HOOK'
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
exec "$ROOT/scripts/delivery-gate.sh" --mode push --skip-health
HOOK

chmod +x "$HOOK_DIR/pre-commit" "$HOOK_DIR/pre-push"
echo "[repo-hygiene] installed pre-commit and pre-push hooks in $HOOK_DIR"
