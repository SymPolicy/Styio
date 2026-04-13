# Styio Performance Testing Route

这个文件只保留导航职责。性能/基准的权威说明已经统一收口到根目录 [`benchmark/`]($HOME/DevSpace/Unka-Malloc/styio/benchmark/README.md:1)，后续不要在这里重复维护细节。

主入口：

- 路线说明：[benchmark/README.md]($HOME/DevSpace/Unka-Malloc/styio/benchmark/README.md:1)
- 一键脚本：[benchmark/perf-route.sh]($HOME/DevSpace/Unka-Malloc/styio/benchmark/perf-route.sh:1)
- 二分最小化：[benchmark/soak-minimize.sh]($HOME/DevSpace/Unka-Malloc/styio/benchmark/soak-minimize.sh:1)
- shadow gate：[benchmark/parser-shadow-suite-gate.sh]($HOME/DevSpace/Unka-Malloc/styio/benchmark/parser-shadow-suite-gate.sh:1)

推荐直接使用：

```bash
./benchmark/perf-route.sh --phase-iters 5000 --micro-iters 5000 --execute-iters 20
```
