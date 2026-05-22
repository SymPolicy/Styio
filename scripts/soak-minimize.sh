#!/usr/bin/env bash
set -euo pipefail

# MIGRATION-NEEDED: M-SCRIPT-02 (docs/rollups/MIGRATION-LEDGER.md)
# Compatibility wrapper retained for one release cycle. The canonical
# entrypoint is benchmark/soak-minimize.sh and there are no remaining
# callers of this wrapper outside docs prose. Delete after the cycle.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$script_dir/../benchmark/soak-minimize.sh" "$@"
