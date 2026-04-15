#!/usr/bin/env bash
set -euo pipefail

# Compatibility wrapper. The canonical entrypoint lives under benchmark/.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$script_dir/../benchmark/parser-shadow-suite-gate.sh" "$@"
