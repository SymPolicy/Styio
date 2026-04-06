# Styio Fuzz Targets

## Build

```bash
cmake -S . -B build-fuzz \
  -DSTYIO_ENABLE_FUZZ=ON \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm@18/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm@18/bin/clang++ \
  -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)" \
  -DCMAKE_CXX_FLAGS='-stdlib=libc++' \
  -DCMAKE_EXE_LINKER_FLAGS='-stdlib=libc++' \
  -DLLVM_DIR=/opt/homebrew/opt/llvm@18/lib/cmake/llvm \
  -DCMAKE_PREFIX_PATH='/opt/homebrew/opt/llvm@18;/opt/homebrew/opt/icu4c@78'
cmake --build build-fuzz --target styio_fuzz_lexer styio_fuzz_parser
```

## Smoke Run

```bash
ctest --test-dir build-fuzz -L fuzz_smoke --output-on-failure
```

`fuzz_smoke` 已在 CTest 中注入 `ASAN_OPTIONS=detect_container_overflow=0`，用于规避当前工具链下 libFuzzer 运行时 container-overflow 误报噪音。
同一标签下还包含 `fuzz_regression_pack_smoke`，用于验证失败样本打包与回流模板生成链路。
`fuzz_lexer_smoke` / `fuzz_parser_smoke` 会先复制 corpus 到临时目录再执行，避免污染仓库内 `tests/fuzz/corpus/`。

## Manual Run

```bash
./build-fuzz/bin/styio_fuzz_lexer tests/fuzz/corpus/lexer -runs=10000
./build-fuzz/bin/styio_fuzz_parser tests/fuzz/corpus/parser -runs=10000
```

## Nightly Case Pack

```bash
./scripts/fuzz-regression-pack.sh \
  --artifacts-root ./fuzz-artifacts \
  --out-dir ./fuzz-regressions \
  --run-id local-manual
```

产物目录包含：

- `summary.json`：样本计数与元数据
- `manifest.tsv`：崩溃样本到规范化 seed 的映射
- `CASE.md`：复现与后续动作模板
- `apply-corpus-backflow.sh`：将 seed 回流到 `tests/fuzz/corpus/`
