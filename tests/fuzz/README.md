# Styio Fuzz Targets

## Recommended Gate

```bash
bash scripts/ide-fuzz-gate.sh
```

This gate configures the Clang/libFuzzer build, reuses already-populated local Tree-sitter and GoogleTest source checkouts when available, builds `styio_fuzz_suite`, and runs `ctest -L fuzz_smoke`.

## Build

```bash
cmake -S . -B build-fuzz \
  -DSTYIO_ENABLE_FUZZ=ON \
  -DCMAKE_C_COMPILER=<homebrew-prefix>/opt/llvm@18/bin/clang \
  -DCMAKE_CXX_COMPILER=<homebrew-prefix>/opt/llvm@18/bin/clang++ \
  -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)" \
  -DCMAKE_CXX_FLAGS='-stdlib=libc++' \
  -DCMAKE_EXE_LINKER_FLAGS='-stdlib=libc++' \
  -DLLVM_DIR=<homebrew-prefix>/opt/llvm@18/lib/cmake/llvm \
  -DCMAKE_PREFIX_PATH='<homebrew-prefix>/opt/llvm@18;<homebrew-prefix>/opt/icu4c@78' \
  -DFETCHCONTENT_SOURCE_DIR_TREE_SITTER_RUNTIME="$PWD/build-codex/_deps/tree_sitter_runtime-src"
cmake --build build-fuzz --target styio_fuzz_suite
```

## Smoke Run

```bash
ctest --test-dir build-fuzz -L fuzz_smoke --output-on-failure
```

`fuzz_smoke` 现在通过独立的 corpus-replay smoke binaries 顺序回放 seed，避免把 libFuzzer 入口本身的启动差异混进 PR 级门禁。
`fuzz_smoke` 已在 CTest 中注入 `ASAN_OPTIONS=detect_container_overflow=0`，用于兼容仍由 libFuzzer 构建出的目标二进制和现有环境变量约束。
同一标签下还包含 `fuzz_regression_pack_smoke`，用于验证失败样本打包与回流模板生成链路。
`fuzz_lexer_smoke` / `fuzz_parser_smoke` 会先复制 corpus 到临时目录再执行，避免污染仓库内 `tests/fuzz/corpus/`。
`styio_fuzz_parser` 当前会对同一输入顺序驱动 `legacy` 与 `nightly` 两条 parser 路由，不再只 fuzz legacy。
`StyioFuzzTargets.SyntaxCompletionAndLspSyncRemainStable` 会依次运行 syntax/completion/LSP sync 三个 IDE fuzz target。

## Manual Run

```bash
./build-fuzz/bin/styio_fuzz_lexer tests/fuzz/corpus/lexer -runs=10000
./build-fuzz/bin/styio_fuzz_parser tests/fuzz/corpus/parser -runs=10000
./build-fuzz/bin/styio_fuzz_ide_syntax tests/fuzz/corpus/ide_syntax -runs=10000
./build-fuzz/bin/styio_fuzz_ide_completion tests/fuzz/corpus/ide_completion -runs=10000
./build-fuzz/bin/styio_fuzz_ide_lsp_sync tests/fuzz/corpus/ide_lsp_sync -runs=10000
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
