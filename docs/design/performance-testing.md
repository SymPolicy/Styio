# Styio Performance Testing Route

**Purpose:** Provide a lightweight pointer from `docs/design/` to the benchmark workflow under [`benchmark/`](../../benchmark/README.md), without duplicating the benchmark SSOT.

**Last updated:** 2026-04-14

这个文件只保留导航职责。性能/基准的权威说明已经统一收口到根目录 [`benchmark/`](<user-home>/DevSpace/styio-org/styio/benchmark/README.md:1)，后续不要在这里重复维护细节。

主入口：

- 路线说明：[benchmark/README.md](<user-home>/DevSpace/styio-org/styio/benchmark/README.md:1)
- 一键脚本：[benchmark/perf-route.sh](<user-home>/DevSpace/styio-org/styio/benchmark/perf-route.sh:1)
- 二分最小化：[benchmark/soak-minimize.sh](<user-home>/DevSpace/styio-org/styio/benchmark/soak-minimize.sh:1)
- shadow gate：[benchmark/parser-shadow-suite-gate.sh](<user-home>/DevSpace/styio-org/styio/benchmark/parser-shadow-suite-gate.sh:1)

推荐直接使用：

```bash
./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20 --error-iters 50
```

该脚本会把结构化结果落到 `benchmark/reports/<timestamp>/`，摘要和覆盖说明见：

- [benchmark/README.md](<user-home>/DevSpace/styio-org/styio/benchmark/README.md:1)
- [benchmark/COVERAGE-MATRIX.md](<user-home>/DevSpace/styio-org/styio/benchmark/COVERAGE-MATRIX.md:1)
