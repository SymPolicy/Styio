# Styio Performance Testing Route

**Purpose:** Point the compiler checkout at the external parity-v2 authority;
workloads, runner, reports, and thresholds remain in `styio-benchmark`.

**Last updated:** 2026-08-10

The standards-derived contract is
`styio-benchmark/workloads/parity-v2/contract.json`. It freezes independent
CLBG and LLVM TestSuite-style families, official reference scales, equivalent
work units, and the closed route set `compile-and-run`, `native-build`, and
`native-run`. Reduced smoke/development inputs are never presented as official
parity evidence.

Validate the contract from the benchmark checkout:

```bash
python3 tools/standard_parity_gate.py catalog-check \
  --contract workloads/parity-v2/contract.json
python3 tools/standard_parity_gate.py cpp-strength \
  --contract workloads/parity-v2/contract.json
```

Run bounded family shards with the current Styio build, then merge and verify
only complete reference-scale reports:

```bash
python3 tools/standard_parity_gate.py run \
  --contract workloads/parity-v2/contract.json \
  --family llvm-scalar-chain --scale smoke \
  --styio-root /path/to/styio-nightly --build-dir /path/to/styio-nightly/build \
  --out-dir reports/standard-parity/shards/llvm-scalar-chain \
  --warmups 3 --repetitions 11
python3 tools/standard_parity_gate.py merge \
  --contract workloads/parity-v2/contract.json \
  --reports-dir reports/standard-parity/shards \
  --out-dir reports/standard-parity/final
python3 tools/standard_parity_gate.py verify \
  --contract workloads/parity-v2/contract.json \
  --report reports/standard-parity/final/results.json \
  --require-all --privacy strict
```

The compiler checkout supplies the Styio executable and existing phase probe;
it does not maintain a second benchmark workload catalog or a compatibility
route. Reports are privacy-safe and must not contain host identity, private
paths, commands, endpoints, secrets, or raw subprocess text.

For compiler-owned native-build attribution, `styio build` may reuse runtime
and generated user objects through the configured
`STYIO_NATIVE_RUNTIME_CACHE_DIR`; set `STYIO_NATIVE_CACHE=0` for a cold
diagnostic. Cache keys include compiler identity, target, ABI, complete flags,
runtime/header inputs, and generated IR/wrapper content. Invalid or failed
entries are discarded and the original source-link route remains the fallback.
Set `STYIO_NATIVE_BUILD_PROFILE_OUT` or `STYIO_NATIVE_PROFILE_OUT` only for
diagnostic runs outside timed parity samples; their JSON is phase attribution,
not parity evidence.
