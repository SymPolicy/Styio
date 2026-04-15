#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/cleanup-builds.sh [options]

Options:
  --keep <count>  Keep the newest <count> build directories (default: 3)
  --apply         Delete old build directories. Without this flag, only print a dry run.
  -h, --help      Show this help

Notes:
  - Matches build directories in the repo root named "build" or "build-*".
  - Sorts by directory modification time, newest first.
USAGE
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

KEEP=3
APPLY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --keep)
      if [[ $# -lt 2 ]]; then
        echo "$0: --keep requires a value" >&2
        exit 2
      fi
      KEEP="$2"
      shift 2
      ;;
    --apply)
      APPLY=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if ! [[ "$KEEP" =~ ^[0-9]+$ ]] || [[ "$KEEP" -lt 1 ]]; then
  echo "$0: --keep must be a positive integer" >&2
  exit 2
fi

stat_mtime() {
  local path="$1"

  if stat -f '%m' "$path" >/dev/null 2>&1; then
    stat -f '%m' "$path"
    return 0
  fi

  stat -c '%Y' "$path"
}

format_mtime() {
  local epoch="$1"

  if date -r "$epoch" '+%Y-%m-%d %H:%M:%S' >/dev/null 2>&1; then
    date -r "$epoch" '+%Y-%m-%d %H:%M:%S'
    return 0
  fi

  date -d "@$epoch" '+%Y-%m-%d %H:%M:%S'
}

dir_size() {
  local path="$1"
  du -sh "$path" 2>/dev/null | awk '{print $1}'
}

declare -a entries=()
while IFS= read -r -d '' dir; do
  rel="${dir#$ROOT/}"
  entries+=("$(stat_mtime "$dir")"$'\t'"$rel")
done < <(find "$ROOT" -mindepth 1 -maxdepth 1 -type d \( -name 'build' -o -name 'build-*' \) -print0)

if [[ ${#entries[@]} -eq 0 ]]; then
  echo "[cleanup-builds] no build directories found under $ROOT"
  exit 0
fi

declare -a sorted=()
while IFS= read -r line; do
  sorted+=("$line")
done < <(printf '%s\n' "${entries[@]}" | sort -t $'\t' -k1,1nr -k2,2)

echo "[cleanup-builds] repo root: $ROOT"
echo "[cleanup-builds] keep newest: $KEEP"
echo "[cleanup-builds] mode: $([[ "$APPLY" -eq 1 ]] && echo apply || echo dry-run)"
echo

declare -a remove_dirs=()
for i in "${!sorted[@]}"; do
  line="${sorted[$i]}"
  epoch="${line%%$'\t'*}"
  rel="${line#*$'\t'}"
  abs="$ROOT/$rel"
  size="$(dir_size "$abs")"
  label="KEEP"

  if (( i >= KEEP )); then
    label="REMOVE"
    remove_dirs+=("$abs")
  fi

  printf '[cleanup-builds] %-6s %s  %8s  %s\n' \
    "$label" \
    "$(format_mtime "$epoch")" \
    "$size" \
    "$rel"
done

if [[ ${#remove_dirs[@]} -eq 0 ]]; then
  echo
  echo "[cleanup-builds] nothing to delete"
  exit 0
fi

if [[ "$APPLY" -ne 1 ]]; then
  echo
  echo "[cleanup-builds] dry run only; re-run with --apply to delete old build directories"
  exit 0
fi

echo
for dir in "${remove_dirs[@]}"; do
  echo "[cleanup-builds] deleting ${dir#$ROOT/}"
  rm -rf -- "$dir"
done

echo "[cleanup-builds] cleanup complete"
