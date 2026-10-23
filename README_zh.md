# Styio

Styio 是一门实验性的通用可视化编程语言，采用符号语法表达数据流。

当前仓库承载 nightly 编译器、CLI、资源拓扑语义、测试与仓库内文档。这个分支以源码构建和开发验证为主，不承诺公开二进制发布物。

[English README](README.md) | [构建指南](docs/BUILD-AND-DEV-ENV.md) | [仓库文档](docs/README.md) | [示例](example/README.md)

## 语言设计

Styio 用符号语法表达数据源、变换、分支与输出目标。可视化开发环境的设计目标是结合编译器生成的结构与流程视图，以及运行时执行信息。

编译器当前通过可选的观察接口导出经过验证的资源拓扑。[语言设计](docs/design/Styio-Language-Design.md#24-visual-design-intent)说明完整程序视图的目标，[可观察契约](docs/design/Styio-Observable-Language.md#3-current-compiler-foundation)列出已实现的能力。本段为英文规范的简要说明。

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

环境引导脚本同时支持 Debian/Ubuntu 与 macOS；macOS 下使用 Homebrew LLVM
的完整配置命令见构建指南。

```bash
scripts/bootstrap-dev-env.sh --help
cmake -S . -B build/default -DCMAKE_BUILD_TYPE=Debug
cmake --build build/default --parallel --target \
  styio styio_nano styio_test styio_security_test \
  styio_resource_topology_test styio_algorithm_equivalence_test \
  styio_newparser_internal_test styio_parser_internal_test \
  styio_platform_internal_test styio_native_interop_internal_test
ctest --test-dir build/default -L security --output-on-failure --no-tests=error
ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error
```

完整环境说明见 [docs/BUILD-AND-DEV-ENV.md](docs/BUILD-AND-DEV-ENV.md)。

## 可运行示例

```bash
build/default/bin/styio --file example/hello_world.styio
printf '[3, 1, 2]\n' | build/default/bin/styio --file example/algorithms/bubble_sort.styio
STYIO_BIN=build/default/bin/styio ./example/cli_calculator.sh "1 + 2 * (3 + 4)"
```

示例包括经典算法和流式应用，也为评估类型推导、函数组合及代码冗余提供可执行用例。

`example/` 目录只保留当前可运行并由 CTest 覆盖的示例；暂未实现的语言草稿在稳定规则提升到文档后不保留在当前树。

## 仓库边界

本仓库负责编译器、语言测试、资源拓扑语义和 CLI 合同。包管理、托管服务与可视化工具由独立仓库维护，不能在没有本地合同的情况下视为本仓库已实现能力。

贡献、支持、安全报告和发布规则见 [CONTRIBUTING.md](CONTRIBUTING.md)、[SECURITY.md](SECURITY.md)、[SUPPORT.md](SUPPORT.md) 与 [RELEASE-POLICY.md](RELEASE-POLICY.md)。
