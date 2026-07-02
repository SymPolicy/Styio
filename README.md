# Styio

**Styio is an experimental symbolic language for stream processing, resource
topology, and intent-oriented execution.**

[![CI](https://img.shields.io/github/actions/workflow/status/styio-org/styio-nightly/styio-ci-gate.yml?branch=nightly&style=flat-square&logo=github&label=ci)](https://github.com/styio-org/styio-nightly/actions/workflows/styio-ci-gate.yml)
[![License](https://img.shields.io/github/license/styio-org/styio-nightly?style=flat-square)](https://github.com/styio-org/styio-nightly/blob/nightly/LICENSE)

[Simplified Chinese](README_zh.md) | [Build guide](docs/BUILD-AND-DEV-ENV.md) | [Repository docs](docs/README.md) | [Examples](example/README.md)

This repository carries the nightly compiler, CLI, resource-topology runtime
model, tests, and repository-local documentation. It is intended for source
builds and active development. Public binary release artifacts are not part of
this branch contract.

## A Glimpse of Styio

This runnable example reads two prices from `@stdin`, starts two independent
jobs, awaits both results, and writes one combined signal.

```styio
price_a, price_b <- @stdin : (f64, f64)

||> [
    spread_job := { <| price_a - price_b }
    midpoint_job := { <| (price_a + price_b) / 2.0 }
]

?| spread_job -> spread: f64 | 0.0
?| midpoint_job -> midpoint: f64 | 0.0

?(spread > 5.0 || spread < -5.0) => {
    signal = ("parallel signal: spread=" + spread) + ", midpoint=" + midpoint
    signal -> @stdout
}
```

Run it from the repository root after building `styio`:

```bash
printf '101\n94\n' | build/default/bin/styio --file example/job_parallel_signal.styio
```

Expected output:

```text
parallel signal: spread=7.000000, midpoint=97.500000
```

## Build From Source

Install the documented dependencies, configure the default build directory, and
run the main local gates:

```bash
scripts/bootstrap-dev-env.sh --help
cmake -S . -B build/default -DCMAKE_BUILD_TYPE=Debug
cmake --build build/default --target styio styio_test styio_security_test styio_resource_topology_test -j"$(nproc)"
ctest --test-dir build/default -L security --output-on-failure
ctest --test-dir build/default -L styio_pipeline --output-on-failure
```

For the full environment contract, see
[docs/BUILD-AND-DEV-ENV.md](docs/BUILD-AND-DEV-ENV.md).

## Useful Examples

```bash
build/default/bin/styio --file example/hello_world.styio
printf '[3, 1, 2]\n' | build/default/bin/styio --file example/algorithms/bubble_sort.styio
STYIO_BIN=build/default/bin/styio ./example/cli_calculator.sh "1 + 2 * (3 + 4)"
```

The active examples under `example/` are covered by CTest. Draft language ideas
that do not currently run should stay out of the active tree after their durable
rules are promoted into docs.

## Project Boundaries

Styio is developed alongside related repositories, but this repository owns the
compiler, language tests, resource-topology semantics, and CLI contracts. Package
management, hosted services, and visual tooling are maintained in separate
repositories and should not be treated as implemented by this checkout unless a
local contract says so.

## Contributing And Security

Read [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), and
[SUPPORT.md](SUPPORT.md) before filing issues or proposing changes. Release and
dependency rules are tracked in [RELEASE-POLICY.md](RELEASE-POLICY.md) and
[DEPENDENCY-USAGE.md](DEPENDENCY-USAGE.md).

## License

Apache-2.0. See [LICENSE](LICENSE).
