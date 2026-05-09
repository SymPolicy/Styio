#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
styio_root="$(cd "${script_dir}/.." && pwd)"

find_benchmark_root() {
  if [[ -n "${STYIO_BENCHMARK_ROOT:-}" ]]; then
    if [[ -x "${STYIO_BENCHMARK_ROOT}/tools/soak-minimize.sh" ]]; then
      cd "${STYIO_BENCHMARK_ROOT}" && pwd
      return 0
    fi
    echo "STYIO_BENCHMARK_ROOT does not contain tools/soak-minimize.sh: ${STYIO_BENCHMARK_ROOT}" >&2
    return 1
  fi

  local candidates=(
    "${styio_root}/../styio-benchmark"
    "${styio_root}/../../SymPolicy/styio-benchmark"
    "${HOME:-}/SymPolicy/styio-benchmark"
  )
  local candidate
  for candidate in "${candidates[@]}"; do
    [[ -n "${candidate}" ]] || continue
    if [[ -x "${candidate}/tools/soak-minimize.sh" ]]; then
      cd "${candidate}" && pwd
      return 0
    fi
  done

  echo "Unable to find styio-benchmark. Set STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark." >&2
  return 1
}

benchmark_root="$(find_benchmark_root)"
exec "${benchmark_root}/tools/soak-minimize.sh" --styio-root "${styio_root}" "$@"
