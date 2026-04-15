#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/fuzz-regression-pack.sh [options]

Options:
  --artifacts-root <path>  Fuzz artifact root (default: ./fuzz-artifacts)
  --out-dir <path>         Packed regression output root (default: ./fuzz-regressions)
  --corpus-root <path>     Fuzz corpus root (default: ./tests/fuzz/corpus)
  --run-id <id>            Stable run id for output folder naming
  --copy-into-corpus       Copy normalized crash seeds into <corpus-root>/<target>/
  -h, --help               Show this help
EOF
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_ROOT="$ROOT/fuzz-artifacts"
OUT_DIR="$ROOT/fuzz-regressions"
CORPUS_ROOT="$ROOT/tests/fuzz/corpus"
RUN_ID=""
COPY_INTO_CORPUS=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --artifacts-root)
      ARTIFACT_ROOT="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --corpus-root)
      CORPUS_ROOT="$2"
      shift 2
      ;;
    --run-id)
      RUN_ID="$2"
      shift 2
      ;;
    --copy-into-corpus)
      COPY_INTO_CORPUS=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

hash_file() {
  local file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  else
    shasum -a 256 "$file" | awk '{print $1}'
  fi
}

timestamp="$(date +"%Y%m%d-%H%M%S")"
if [[ -n "$RUN_ID" ]]; then
  run_slug="$RUN_ID"
else
  run_slug="${timestamp}-nightly-fuzz"
fi

case_dir="$OUT_DIR/$run_slug"
if [[ -e "$case_dir" ]]; then
  case_dir="${case_dir}-${timestamp}"
fi
mkdir -p "$case_dir"

manifest="$case_dir/manifest.tsv"
printf "target\tsha256\tsize\tartifact\tnormalized_seed\n" > "$manifest"

lexer_count=0
parser_count=0
total_count=0

sample_patterns=(
  -name 'crash-*'
  -o -name 'timeout-*'
  -o -name 'leak-*'
  -o -name 'oom-*'
  -o -name 'slow-unit-*'
)

for target in lexer parser; do
  log_path="$ARTIFACT_ROOT/${target}.log"
  if [[ -f "$log_path" ]]; then
    cp "$log_path" "$case_dir/${target}.log"
  fi

  sample_dir="$ARTIFACT_ROOT/$target"
  if [[ ! -d "$sample_dir" ]]; then
    continue
  fi

  target_raw_dir="$case_dir/$target/raw"
  target_seed_dir="$case_dir/$target/corpus-backflow"
  mkdir -p "$target_raw_dir" "$target_seed_dir"

  while IFS= read -r -d '' sample; do
    base_name="$(basename "$sample")"
    sha="$(hash_file "$sample")"
    size_bytes="$(wc -c < "$sample" | tr -d ' ')"
    normalized_seed="$target_seed_dir/${sha}.seed"

    cp "$sample" "$target_raw_dir/$base_name"
    cp "$sample" "$normalized_seed"

    printf "%s\t%s\t%s\t%s\t%s\n" \
      "$target" "$sha" "$size_bytes" "$target/raw/$base_name" "$target/corpus-backflow/${sha}.seed" \
      >> "$manifest"

    if [[ "$COPY_INTO_CORPUS" -eq 1 ]]; then
      mkdir -p "$CORPUS_ROOT/$target"
      cp "$sample" "$CORPUS_ROOT/$target/${sha}.seed"
    fi

    total_count=$((total_count + 1))
    if [[ "$target" == "lexer" ]]; then
      lexer_count=$((lexer_count + 1))
    else
      parser_count=$((parser_count + 1))
    fi
  done < <(find "$sample_dir" -maxdepth 1 -type f \( "${sample_patterns[@]}" \) -print0 | sort -z)
done

cat > "$case_dir/summary.json" <<EOF
{
  "run_id": "$run_slug",
  "generated_at": "$(date +"%Y-%m-%dT%H:%M:%S%z")",
  "artifact_root": "$(realpath "$ARTIFACT_ROOT")",
  "samples": {
    "total": $total_count,
    "lexer": $lexer_count,
    "parser": $parser_count
  }
}
EOF

cat > "$case_dir/apply-corpus-backflow.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORPUS_ROOT="${1:-$(cd "$CASE_DIR/../.." && pwd)/tests/fuzz/corpus}"

for target in lexer parser; do
  src="$CASE_DIR/$target/corpus-backflow"
  dst="$CORPUS_ROOT/$target"
  [[ -d "$src" ]] || continue
  mkdir -p "$dst"
  cp "$src"/* "$dst"/
  echo "[fuzz-backflow] copied ${target} seeds into $dst"
done
EOF
chmod +x "$case_dir/apply-corpus-backflow.sh"

if [[ -f "$ROOT/tests/fuzz/REGRESSION-TEMPLATE.md" ]]; then
  cp "$ROOT/tests/fuzz/REGRESSION-TEMPLATE.md" "$case_dir/REGRESSION-TEMPLATE.md"
fi

cat > "$case_dir/CASE.md" <<EOF
# Fuzz Regression Case Pack

- Run id: $run_slug
- Generated at: $(date +"%Y-%m-%d %H:%M:%S %z")
- Artifact root: $(realpath "$ARTIFACT_ROOT")
- Collected samples: $total_count (lexer=$lexer_count, parser=$parser_count)

## Files

- \`summary.json\`: aggregate metadata
- \`manifest.tsv\`: per-sample mapping (target, sha256, size, source, normalized seed)
- \`<target>.log\`: deep fuzz execution logs (if present)
- \`<target>/raw/\`: raw libFuzzer artifacts (\`crash-*\`, \`timeout-*\`, ...)
- \`<target>/corpus-backflow/\`: normalized seeds ready for corpus backflow
- \`apply-corpus-backflow.sh\`: helper to copy normalized seeds into \`tests/fuzz/corpus/\`
- \`REGRESSION-TEMPLATE.md\`: fill-in template for converting samples into stable tests

## Quick Repro

\`\`\`bash
# Re-run parser deep fuzz locally (example)
./<build-dir>/bin/styio_fuzz_parser tests/fuzz/corpus/parser -max_total_time=600

# Replay one artifact directly (replace with an actual file)
./<build-dir>/bin/styio_fuzz_parser $case_dir/parser/raw/<artifact> -runs=1
\`\`\`

## Next Actions

1. Pick highest-value samples from \`manifest.tsv\` and add them to tracked corpus.
2. Convert at least one sample into a deterministic regression test using \`REGRESSION-TEMPLATE.md\`.
3. Link the fixing commit / CI run URL back to this case pack.
EOF

echo "[fuzz-pack] output: $case_dir"
echo "[fuzz-pack] samples: total=$total_count lexer=$lexer_count parser=$parser_count"
