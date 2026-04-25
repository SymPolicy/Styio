#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/delivery-gate.sh [options]

Run the common Styio delivery floor through the workflow scheduler, then run
checkpoint health when requested.

Options:
  --mode <checkpoint|push>  Delivery mode (default: checkpoint)
  --base <ref>              Base ref for team-docs-gate branch checks
  --range <rev-range>       Explicit revision range for repo-hygiene push mode
  --skip-health             Skip checkpoint-health (docs/process-only deliveries)
  --with-asan               Include the ASan/UBSan leg in checkpoint-health
  --with-fuzz               Include the fuzz-smoke leg in checkpoint-health
  --build-dir <dir>         Forwarded to checkpoint-health
  --asan-build-dir <dir>    Forwarded to checkpoint-health; also enables ASan
  --fuzz-build-dir <dir>    Forwarded to checkpoint-health; also enables fuzz
  -h, --help                Show this help
USAGE
}

log() {
  echo "[delivery-gate] $*"
}

run_cmd() {
  log "$*"
  "$@"
}

default_upstream_base() {
  git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' 2>/dev/null || true
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

MODE="checkpoint"
BASE_REF=""
REV_RANGE=""
RUN_HEALTH=1
RUN_ASAN=0
RUN_FUZZ=0
BUILD_DIR=""
ASAN_BUILD_DIR=""
FUZZ_BUILD_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)
      MODE="$2"
      shift 2
      ;;
    --base)
      BASE_REF="$2"
      shift 2
      ;;
    --range)
      REV_RANGE="$2"
      shift 2
      ;;
    --skip-health)
      RUN_HEALTH=0
      shift
      ;;
    --with-asan)
      RUN_ASAN=1
      shift
      ;;
    --with-fuzz)
      RUN_FUZZ=1
      shift
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --asan-build-dir)
      ASAN_BUILD_DIR="$2"
      RUN_ASAN=1
      shift 2
      ;;
    --fuzz-build-dir)
      FUZZ_BUILD_DIR="$2"
      RUN_FUZZ=1
      shift 2
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

case "$MODE" in
  checkpoint|push)
    ;;
  *)
    echo "Unsupported mode: $MODE" >&2
    usage >&2
    exit 2
    ;;
esac

SCHEDULER_CMD=(python3 scripts/workflow-scheduler.py run)
HEALTH_CMD=(./scripts/checkpoint-health.sh)

if [[ "$MODE" == "checkpoint" ]]; then
  SCHEDULER_CMD+=(--profile delivery-checkpoint)
else
  SCHEDULER_CMD+=(--profile delivery-push)
  if [[ -n "$REV_RANGE" ]]; then
    SCHEDULER_CMD+=(--range "$REV_RANGE")
  fi

  if [[ -z "$BASE_REF" ]]; then
    BASE_REF="$(default_upstream_base)"
  fi

  if [[ -z "$BASE_REF" ]]; then
    echo "push mode requires --base <ref> or a configured upstream branch for team-docs-gate" >&2
    exit 2
  fi

  SCHEDULER_CMD+=(--base "$BASE_REF")
fi

if [[ -n "$BASE_REF" && "$MODE" == "checkpoint" ]]; then
  echo "checkpoint mode does not accept --base; use --mode push for branch checks" >&2
  exit 2
fi

if [[ -n "$BUILD_DIR" ]]; then
  HEALTH_CMD+=(--build-dir "$BUILD_DIR")
fi
if [[ -n "$ASAN_BUILD_DIR" ]]; then
  HEALTH_CMD+=(--asan-build-dir "$ASAN_BUILD_DIR")
fi
if [[ -n "$FUZZ_BUILD_DIR" ]]; then
  HEALTH_CMD+=(--fuzz-build-dir "$FUZZ_BUILD_DIR")
fi
if [[ "$RUN_ASAN" -eq 0 ]]; then
  HEALTH_CMD+=(--no-asan)
fi
if [[ "$RUN_FUZZ" -eq 0 ]]; then
  HEALTH_CMD+=(--no-fuzz)
fi

log "mode: ${MODE}"
run_cmd "${SCHEDULER_CMD[@]}"

if [[ "$RUN_HEALTH" -eq 1 ]]; then
  run_cmd "${HEALTH_CMD[@]}"
else
  log "checkpoint-health skipped"
fi

log "all checks passed"
