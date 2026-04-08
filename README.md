# Styio

**文档作用：** 仓库入口（CLI 示例与测试脚手架脚本）；语言规格、里程碑与文档维护准则见 `docs/`（建议先读 [`docs/README.md`](docs/README.md)、[`docs/specs/DOCUMENTATION-POLICY.md`](docs/specs/DOCUMENTATION-POLICY.md) §0、[`docs/assets/workflow/CHECKPOINT-WORKFLOW.md`](docs/assets/workflow/CHECKPOINT-WORKFLOW.md) 与 [`docs/specs/AGENT-SPEC.md`](docs/specs/AGENT-SPEC.md)）。

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

## Diagnostics

- 默认：`--error-format=text`，错误输出到 `stderr`。
- 机器可读：`--error-format=jsonl`，每条错误输出一行 JSON。

```bash
./build/bin/styio --error-format=jsonl --file tests/milestones/m5/t06_fail_fast.styio 2>&1
```

```json
{"category":"RuntimeError","code":"STYIO_RUNTIME","subcode":"STYIO_RUNTIME_FILE_OPEN_READ","file":"...","message":"cannot open file for read: ..."}
```

- `category`：`LexError|ParseError|TypeError|RuntimeError`
- `code`：`STYIO_LEX|STYIO_PARSE|STYIO_TYPE|STYIO_RUNTIME`
- `subcode`：仅在细分类可用时输出（当前 runtime 子类会输出稳定 `subcode`）

### extend_tests.py
Python script for creating test files.

```
python3 extend_tests.py
```
