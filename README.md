# Styio

**文档作用：** 仓库入口（CLI 示例与测试脚手架脚本）；语言规格、里程碑与文档维护准则见 `docs/`（建议先读 [`docs/DOCUMENTATION-POLICY.md`](docs/DOCUMENTATION-POLICY.md) §0、[`docs/CHECKPOINT-WORKFLOW.md`](docs/CHECKPOINT-WORKFLOW.md) 与 [`docs/AGENT-SPEC.md`](docs/AGENT-SPEC.md)）。

## Build
```bash
cmake -S . -B build
cmake --build build
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
