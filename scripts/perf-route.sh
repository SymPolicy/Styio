#!/usr/bin/env bash
set -euo pipefail

# MIGRATION-NEEDED: M-SCRIPT-01 (docs/rollups/MIGRATION-LEDGER.md)
# Compatibility wrapper retained for one release cycle. The canonical
# entrypoint is benchmark/perf-route.sh and there are no remaining callers
# of this wrapper outside docs prose. Delete after the deprecation cycle.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$script_dir/../benchmark/perf-route.sh" "$@"
