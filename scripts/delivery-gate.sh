#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/delivery-gate.sh [options]

Run the common Styio delivery floor through the workflow scheduler, then run
external audit and checkpoint health when requested.

Options:
  --mode <auto|checkpoint|staged|push|release>
                            Delivery mode (default: auto)
  --base <ref>              Base ref for team-docs-gate branch checks
  --range <rev-range>       Explicit revision range for repo-hygiene push mode
  --skip-health             Skip checkpoint-health (docs/process-only deliveries)
  --skip-audit              Skip external styio-audit gate
  --audit-bin <path>        Explicit styio-audit executable
  --with-asan               Include the ASan/UBSan leg in checkpoint-health
  --with-fuzz               Include the fuzz-smoke leg in checkpoint-health
  --build-dir <dir>         Forwarded to checkpoint-health
  --asan-build-dir <dir>    Forwarded to checkpoint-health; also enables ASan
  --fuzz-build-dir <dir>    Forwarded to checkpoint-health; also enables fuzz
  --coverage-build-dir <dir>
                            Forwarded to checkpoint-health coverage gate
  --coverage-threshold <percent>
                            Minimum line coverage percentage (default: 95)
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

commit_readiness_prompt() {
  log "commit readiness: read workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md before committing functional changes."
  log "commit readiness prompt: Have upstream and downstream surfaces been verified against this changed feature, and does the expected behavior pass?"
  log "commit readiness prompt: Are changed feature/module/workflow/skill/doc names free of version-style placeholders and named by the feature or transformation result?"
  log "commit readiness prompt: If not, keep fixing. If verification is objectively unavailable, record blocker, owner, substitute evidence, and follow-up gate before commit."
}

confirm_commit_readiness_if_needed() {
  local answer

  commit_readiness_prompt

  if [[ "$MODE" != "staged" ]]; then
    return 0
  fi

  if [[ "${STYIO_COMMIT_READINESS_CONFIRMED:-}" == "1" ]]; then
    log "commit readiness confirmation: STYIO_COMMIT_READINESS_CONFIRMED=1"
    return 0
  fi

  if [[ "${STYIO_COMMIT_READINESS_REQUIRE:-}" != "1" ]]; then
    log "commit readiness confirmation: reminder only; set STYIO_COMMIT_READINESS_REQUIRE=1 to require confirmation."
    return 0
  fi

  if [[ -t 0 ]]; then
    read -r -p "[delivery-gate] Type yes if this is not a functional change, or targeted feature validation, upstream/downstream checks, and version-style naming checks are done, or objective blockers are recorded: " answer
    if [[ "$answer" == "yes" ]]; then
      return 0
    fi
    echo "[delivery-gate] commit readiness not confirmed; stop before commit." >&2
    exit 2
  fi

  echo "[delivery-gate] commit readiness confirmation required but stdin is non-interactive." >&2
  echo "[delivery-gate] Confirm this is not a functional change, or verify upstream/downstream behavior, check version-style naming, or record objective blockers, then rerun with STYIO_COMMIT_READINESS_CONFIRMED=1 only if true." >&2
  exit 2
}

default_upstream_base() {
  git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' 2>/dev/null || true
}

current_branch() {
  git branch --show-current 2>/dev/null || true
}

ref_exists() {
  git rev-parse --verify --quiet "$1^{commit}" >/dev/null
}

has_worktree_changes() {
  [[ -n "$(git status --porcelain --untracked-files=all)" ]]
}

rev_count() {
  git rev-list --count "$1" 2>/dev/null || echo 0
}

origin_repo_name() {
  local url path
  url="$(git remote get-url origin 2>/dev/null || true)"
  [[ -n "$url" ]] || return 1
  case "$url" in
    git@github.com:*)
      path="${url#git@github.com:}"
      ;;
    https://github.com/*)
      path="${url#https://github.com/}"
      ;;
    http://github.com/*)
      path="${url#http://github.com/}"
      ;;
    *)
      return 1
      ;;
  esac
  path="${path%.git}"
  [[ "$path" == */* ]] || return 1
  echo "$path"
}

github_parent_repo() {
  local repo parent
  command -v gh >/dev/null 2>&1 || return 1
  repo="$(origin_repo_name)" || return 1
  parent="$(
    gh repo view "$repo" --json parent --jq '.parent | if . == null then "" else .owner.login + "/" + .name end' 2>/dev/null || true
  )"
  [[ -n "$parent" ]] || return 1
  echo "$parent"
}

fetch_github_parent_base() {
  local branch parent ref
  branch="$1"
  [[ -n "$branch" ]] || return 1
  parent="$(github_parent_repo)" || return 1
  ref="refs/delivery-base/${parent/\//-}/${branch}"
  if git fetch --no-tags "https://github.com/${parent}.git" "+refs/heads/${branch}:${ref}" >/dev/null 2>&1; then
    echo "$ref"
    return 0
  fi
  return 1
}

resolve_delivery_base() {
  local branch configured parent_ref upstream_ref upstream_branch
  configured="${BASE_REF:-${STYIO_DELIVERY_BASE:-${STYIO_TEAM_DOC_GATE_BASE:-}}}"
  if [[ -n "$configured" ]]; then
    echo "$configured"
    return 0
  fi

  branch="$(current_branch)"
  if [[ -n "$branch" ]]; then
    upstream_ref="refs/remotes/upstream/${branch}"
    if ref_exists "$upstream_ref"; then
      echo "$upstream_ref"
      return 0
    fi
    parent_ref="$(fetch_github_parent_base "$branch" || true)"
    if [[ -n "$parent_ref" ]]; then
      echo "$parent_ref"
      return 0
    fi
  fi

  upstream_branch="$(default_upstream_base)"
  if [[ -n "$upstream_branch" ]]; then
    echo "$upstream_branch"
    return 0
  fi
  return 1
}

resolve_audit_bin() {
  if [[ -n "$AUDIT_BIN" ]]; then
    echo "$AUDIT_BIN"
    return 0
  fi
  if [[ -n "${STYIO_AUDIT_BIN:-}" ]]; then
    echo "$STYIO_AUDIT_BIN"
    return 0
  fi
  if [[ -n "${STYIO_AUDIT_HOME:-}" && -x "${STYIO_AUDIT_HOME}/bin/styio-audit" ]]; then
    echo "${STYIO_AUDIT_HOME}/bin/styio-audit"
    return 0
  fi
  if command -v styio-audit >/dev/null 2>&1; then
    command -v styio-audit
    return 0
  fi
  return 1
}

run_audit_gate() {
  local audit_bin
  audit_bin="$(resolve_audit_bin || true)"
  if [[ -z "$audit_bin" ]]; then
    echo "styio-audit executable not found; set --audit-bin, STYIO_AUDIT_BIN, or use --skip-audit" >&2
    exit 2
  fi
  if [[ -x "$audit_bin" ]]; then
    run_cmd "$audit_bin" gate --repo . --project styio
  else
    run_cmd python3 "$audit_bin" gate --repo . --project styio
  fi
}

run_scheduler_profile() {
  local profile
  profile="$1"
  shift
  case "$profile" in
    delivery-checkpoint)
      run_cmd python3 scripts/workflow-scheduler.py run --profile delivery-checkpoint "$@"
      ;;
    delivery-staged)
      run_cmd python3 scripts/workflow-scheduler.py run --profile delivery-staged "$@"
      ;;
    delivery-push)
      run_cmd python3 scripts/workflow-scheduler.py run --profile delivery-push "$@"
      ;;
    *)
      echo "Unsupported scheduler profile: $profile" >&2
      exit 2
      ;;
  esac
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

MODE="auto"
BASE_REF=""
REV_RANGE="${STYIO_DELIVERY_RANGE:-}"
RUN_HEALTH=1
RUN_AUDIT=1
AUDIT_BIN=""
RUN_ASAN=0
RUN_FUZZ=0
BUILD_DIR=""
ASAN_BUILD_DIR=""
FUZZ_BUILD_DIR=""
COVERAGE_BUILD_DIR=""
COVERAGE_THRESHOLD=""

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
    --skip-audit)
      RUN_AUDIT=0
      shift
      ;;
    --audit-bin)
      AUDIT_BIN="$2"
      shift 2
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
    --coverage-build-dir)
      COVERAGE_BUILD_DIR="$2"
      shift 2
      ;;
    --coverage-threshold)
      COVERAGE_THRESHOLD="$2"
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
  auto|checkpoint|staged|push|release)
    ;;
  *)
    echo "Unsupported mode: $MODE" >&2
    usage >&2
    exit 2
    ;;
esac

if [[ "$MODE" == "release" ]]; then
  RUN_ASAN=1
  RUN_FUZZ=1
fi

HEALTH_CMD=(./scripts/checkpoint-health.sh)

if [[ -n "$BUILD_DIR" ]]; then
  HEALTH_CMD+=(--build-dir "$BUILD_DIR")
fi
if [[ -n "$ASAN_BUILD_DIR" ]]; then
  HEALTH_CMD+=(--asan-build-dir "$ASAN_BUILD_DIR")
fi
if [[ -n "$FUZZ_BUILD_DIR" ]]; then
  HEALTH_CMD+=(--fuzz-build-dir "$FUZZ_BUILD_DIR")
fi
if [[ -n "$COVERAGE_BUILD_DIR" ]]; then
  HEALTH_CMD+=(--coverage-build-dir "$COVERAGE_BUILD_DIR")
fi
if [[ -n "$COVERAGE_THRESHOLD" ]]; then
  HEALTH_CMD+=(--coverage-threshold "$COVERAGE_THRESHOLD")
fi
if [[ "$RUN_ASAN" -eq 0 ]]; then
  HEALTH_CMD+=(--no-asan)
fi
if [[ "$RUN_FUZZ" -eq 0 ]]; then
  HEALTH_CMD+=(--no-fuzz)
fi

log "mode: ${MODE}"
confirm_commit_readiness_if_needed

case "$MODE" in
  auto)
    if has_worktree_changes; then
      run_scheduler_profile delivery-checkpoint
    else
      log "worktree: nothing to check"
    fi

    if [[ -z "$BASE_REF" ]]; then
      BASE_REF="$(resolve_delivery_base || true)"
    fi
    if [[ -z "$BASE_REF" ]]; then
      echo "auto mode could not infer a delivery base; set --base <ref> or STYIO_DELIVERY_BASE" >&2
      exit 2
    fi
    if [[ "$(rev_count "${BASE_REF}..HEAD")" -gt 0 ]]; then
      if [[ -z "$REV_RANGE" ]]; then
        REV_RANGE="${BASE_REF}..HEAD"
      fi
      run_scheduler_profile delivery-push --base "$BASE_REF" --range "$REV_RANGE"
    else
      log "push range ${BASE_REF}..HEAD: nothing to check"
    fi
    ;;
  checkpoint)
    if [[ -n "$BASE_REF" || -n "$REV_RANGE" ]]; then
      echo "checkpoint mode does not accept --base or --range; use --mode push for branch checks" >&2
      exit 2
    fi
    run_scheduler_profile delivery-checkpoint
    ;;
  staged)
    if [[ -n "$BASE_REF" || -n "$REV_RANGE" ]]; then
      echo "staged mode does not accept --base or --range; use --mode push for branch checks" >&2
      exit 2
    fi
    run_scheduler_profile delivery-staged
    ;;
  push)
    if [[ -z "$BASE_REF" ]]; then
      BASE_REF="$(resolve_delivery_base || true)"
    fi
    if [[ -z "$BASE_REF" ]]; then
      echo "push mode requires --base <ref> or an inferable upstream/parent branch" >&2
      exit 2
    fi
    if [[ -z "$REV_RANGE" ]]; then
      REV_RANGE="${BASE_REF}..HEAD"
    fi
    run_scheduler_profile delivery-push --base "$BASE_REF" --range "$REV_RANGE"
    ;;
  release)
    if [[ -n "$BASE_REF" || -n "$REV_RANGE" ]]; then
      echo "release mode does not accept --base or --range; run push mode separately for branch checks" >&2
      exit 2
    fi
    run_scheduler_profile delivery-checkpoint
    ;;
esac

if [[ "$RUN_AUDIT" -eq 1 ]]; then
  run_audit_gate
else
  log "styio-audit skipped"
fi

if [[ "$RUN_HEALTH" -eq 1 ]]; then
  run_cmd "${HEALTH_CMD[@]}"
else
  log "checkpoint-health skipped"
fi

log "all checks passed"
