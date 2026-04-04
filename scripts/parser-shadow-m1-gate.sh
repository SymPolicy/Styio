#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <styio-bin> <m1-dir> <artifact-root>" >&2
  exit 2
fi

styio_bin="$1"
m1_dir="$2"
artifact_root="$3"

if [[ ! -x "$styio_bin" ]]; then
  echo "[shadow-gate] compiler is not executable: $styio_bin" >&2
  exit 2
fi

if [[ ! -d "$m1_dir" ]]; then
  echo "[shadow-gate] m1 dir not found: $m1_dir" >&2
  exit 2
fi

mkdir -p "$artifact_root"

total=0
failed=0
cases=0

for input in "$m1_dir"/t*.styio; do
  [[ -f "$input" ]] || continue
  cases=$((cases + 1))
  base_name="$(basename "$input" .styio)"
  for engine in legacy new; do
    total=$((total + 1))
    run_dir="$artifact_root/${base_name}-${engine}"
    mkdir -p "$run_dir"
    if ! "$styio_bin" \
      --parser-engine="$engine" \
      --parser-shadow-compare \
      --parser-shadow-artifact-dir "$run_dir" \
      --file "$input" \
      >/dev/null 2>"$run_dir/stderr.log"; then
      echo "[shadow-gate] failed: case=${base_name} engine=${engine}" >&2
      failed=$((failed + 1))
    fi
  done
done

if [[ $cases -eq 0 ]]; then
  echo "[shadow-gate] no t*.styio case found under $m1_dir" >&2
  exit 2
fi

jsonl_count="$(find "$artifact_root" -type f -name '*.jsonl' | wc -l | tr -d ' ')"
match_count="$(grep -R --include='*.jsonl' -h '"status":"match"' "$artifact_root" | wc -l | tr -d ' ' || true)"
mismatch_count="$(grep -R --include='*.jsonl' -h '"status":"mismatch"' "$artifact_root" | wc -l | tr -d ' ' || true)"
shadow_error_count="$(grep -R --include='*.jsonl' -h '"status":"shadow_error"' "$artifact_root" | wc -l | tr -d ' ' || true)"

summary_file="$artifact_root/summary.json"
cat >"$summary_file" <<EOF
{
  "cases": $cases,
  "runs": $total,
  "artifacts": $jsonl_count,
  "match": $match_count,
  "mismatch": $mismatch_count,
  "shadow_error": $shadow_error_count,
  "failed_runs": $failed,
  "passed": $([[ "$failed" -eq 0 && "$mismatch_count" -eq 0 && "$shadow_error_count" -eq 0 && "$jsonl_count" -ge "$total" ]] && echo "true" || echo "false")
}
EOF

echo "[shadow-gate] cases=${cases} runs=${total} artifacts=${jsonl_count} match=${match_count} mismatch=${mismatch_count} shadow_error=${shadow_error_count}"

if [[ "$jsonl_count" -lt "$total" ]]; then
  echo "[shadow-gate] missing artifact records: expected at least ${total}, got ${jsonl_count}" >&2
  exit 1
fi

if [[ "$failed" -ne 0 || "$mismatch_count" -ne 0 || "$shadow_error_count" -ne 0 ]]; then
  echo "[shadow-gate] gate failed" >&2
  exit 1
fi

echo "[shadow-gate] gate passed"
