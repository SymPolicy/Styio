# Styio

Styio 是一个实验性的符号化语言项目，当前聚焦于流式处理、资源拓扑与意图式执行表达。

当前仓库承载 nightly 编译器、CLI、资源拓扑语义、测试与仓库内文档。这个分支以源码构建和开发验证为主，不承诺公开二进制发布物。

[English README](README.md) | [构建指南](docs/BUILD-AND-DEV-ENV.md) | [仓库文档](docs/README.md) | [示例](example/README.md)

## 快速感受

下面的示例会从 `@stdin` 读取两个价格，启动两个独立任务，等待结果后合并输出一个信号：

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

构建完成后可从仓库根目录运行：

```bash
printf '101\n94\n' | build/default/bin/styio --file example/job_parallel_signal.styio
```

预期输出：

```text
parallel signal: spread=7.000000, midpoint=97.500000
```

## 从源码构建

```bash
scripts/bootstrap-dev-env.sh --help
cmake -S . -B build/default -DCMAKE_BUILD_TYPE=Debug
cmake --build build/default --target styio styio_test styio_security_test styio_resource_topology_test -j"$(nproc)"
ctest --test-dir build/default -L security --output-on-failure
ctest --test-dir build/default -L styio_pipeline --output-on-failure
```

完整环境说明见 [docs/BUILD-AND-DEV-ENV.md](docs/BUILD-AND-DEV-ENV.md)。

## 可运行示例

```bash
build/default/bin/styio --file example/hello_world.styio
printf '[3, 1, 2]\n' | build/default/bin/styio --file example/algorithms/bubble_sort.styio
STYIO_BIN=build/default/bin/styio ./example/cli_calculator.sh "1 + 2 * (3 + 4)"
```

`example/` 目录只保留当前可运行并由 CTest 覆盖的示例；暂未实现的语言草稿归档在 `docs/archive/`。

## 仓库边界

本仓库负责编译器、语言测试、资源拓扑语义和 CLI 合同。包管理、托管服务与可视化工具由独立仓库维护，不能在没有本地合同的情况下视为本仓库已实现能力。

贡献、支持、安全报告和发布规则见 [CONTRIBUTING.md](CONTRIBUTING.md)、[SECURITY.md](SECURITY.md)、[SUPPORT.md](SUPPORT.md) 与 [RELEASE-POLICY.md](RELEASE-POLICY.md)。
