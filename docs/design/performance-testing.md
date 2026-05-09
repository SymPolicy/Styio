# Styio Performance Testing Route

**Purpose:** Provide a lightweight pointer from `docs/design/` to the external benchmark workflow, without duplicating the benchmark SSOT.

**Last updated:** 2026-05-08

这个文件只保留导航职责。性能/基准的权威说明已经统一收口到 `styio-benchmark`，后续不要在 Styio 仓库重复维护 workload、runner、报告或 baseline。

主入口：

- 路线说明：`styio-benchmark/README.md`
- 覆盖矩阵：`styio-benchmark/docs/COVERAGE-MATRIX.md`
- 一键脚本：`styio-benchmark/tools/perf-route.sh`
- 二分最小化：`styio-benchmark/tools/soak-minimize.sh`
- Styio 本仓库仅保留 C++ probe、parser shadow gate 和兼容 wrapper

推荐直接使用：

```bash
/path/to/styio-benchmark/tools/perf-route.sh \
  --styio-root /path/to/styio \
  --phase-iters 5000 \
  --micro-iters 5000 \
  --execute-iters 20 \
  --error-iters 50
```

兼容入口仍可用于本仓库内操作，但它只会转发到 `styio-benchmark`：

```bash
STYIO_BENCHMARK_ROOT=/path/to/styio-benchmark ./benchmark/perf-route.sh --quick
```

结构化结果应落在 `styio-benchmark/reports/<timestamp>/` 或显式 `--out-dir` 指向的 benchmark 仓库目录。
