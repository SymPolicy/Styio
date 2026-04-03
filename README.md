# Styio

**文档作用：** 仓库入口（CLI 示例与测试脚手架脚本）；语言规格、里程碑与文档维护准则见 `docs/`（建议先读 [`docs/DOCUMENTATION-POLICY.md`](docs/DOCUMENTATION-POLICY.md) §0、[`docs/CHECKPOINT-WORKFLOW.md`](docs/CHECKPOINT-WORKFLOW.md) 与 [`docs/AGENT-SPEC.md`](docs/AGENT-SPEC.md)）。

## Build
```bash
cmake -S . -B build
cmake --build build
```

## Optional Dependencies
- 默认不依赖 ICU（`STYIO_USE_ICU=OFF`）。
- 若需要启用 ICU-backed Unicode 分类（`StyioUnicode`）和 cxxopts Unicode 帮助文本，可启用：
```bash
cmake -S . -B build -DSTYIO_USE_ICU=ON
cmake --build build
```
- 若在 macOS + Homebrew（`icu4c` 为 keg-only）环境中找不到 ICU，可加：
```bash
cmake -S . -B build -DSTYIO_USE_ICU=ON -DICU_ROOT=/opt/homebrew/opt/icu4c
```

# Command-Line Arguments
```
./build/bin/styio --styio-ast --styio-ir --llvm-ir --file a.styio
```

### extend_tests.py
Python script for creating test files.

```
python3 extend_tests.py
```
