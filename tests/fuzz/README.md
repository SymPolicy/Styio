# Styio Fuzz Targets

## Recommended Gate

```bash
bash scripts/ide-fuzz-gate.sh
```

This gate configures the Clang/libFuzzer build, reuses already-populated local Tree-sitter and GoogleTest source checkouts when available, builds `styio_fuzz_suite`, and runs `ctest -L fuzz_smoke`.

## Build

```bash
LLVM_PREFIX="$(brew --prefix llvm@18)"
ICU_PREFIX="$(brew --prefix icu4c@78)"
cmake -S . -B build/fuzz \
  -DSTYIO_ENABLE_FUZZ=ON \
  -DCMAKE_C_COMPILER="$LLVM_PREFIX/bin/clang" \
  -DCMAKE_CXX_COMPILER="$LLVM_PREFIX/bin/clang++" \
  -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)" \
  -DCMAKE_CXX_FLAGS='-stdlib=libc++' \
  -DCMAKE_EXE_LINKER_FLAGS='-stdlib=libc++' \
  -DLLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm" \
  -DCMAKE_PREFIX_PATH="$LLVM_PREFIX;$ICU_PREFIX" \
  -DFETCHCONTENT_SOURCE_DIR_TREE_SITTER_RUNTIME="$PWD/build/default/_deps/tree_sitter_runtime-src"
cmake --build build/fuzz --target styio_fuzz_suite
```

## Smoke Run

```bash
ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure
```

`fuzz_smoke` 现在通过独立的 corpus-replay smoke binaries 顺序回放 seed，避免把 libFuzzer 入口本身的启动差异混进 PR 级门禁。
`fuzz_smoke` 已在 CTest 中注入 `ASAN_OPTIONS=detect_container_overflow=0`，用于兼容仍由 libFuzzer 构建出的目标二进制和现有环境变量约束。
同一标签下还包含 `fuzz_regression_pack_smoke`，用于验证失败样本打包与回流模板生成链路。
`fuzz_lexer_smoke` / `fuzz_parser_smoke` 会先复制 corpus 到临时目录再执行，避免污染仓库内 `tests/fuzz/corpus/`。
`styio_fuzz_parser` 当前会对同一输入顺序驱动 `legacy` 与 `nightly` 两条 parser 路由，不再只 fuzz legacy。
`StyioFuzzTargets.SyntaxCompletionAndLspSyncRemainStable` 会依次运行 syntax/completion/LSP sync 三个 IDE fuzz target。
`fuzz_harness_contract_check` 确认 lexer/parser harness 使用 `tokenizeOwned` 而不是进程级 legacy registry。
`fuzz_lifetime_replay` 在同一进程内重复回放空输入、`>>` 和较长源码，覆盖迭代生命周期。

Lexer 与 parser harness 在每次迭代里把 `StyioTokenStream` 作为源码所有者、把 `CompilationSession` 作为 token 所有者，并在 session 析构后再释放 stream。

## Manual Run

```bash
./build/fuzz/bin/styio_fuzz_lexer tests/fuzz/corpus/lexer \
  -runs=10000 -timeout=10 -max_len=65536 -seed=1 \
  -dict=tests/fuzz/styio.dict -use_value_profile=1
./build/fuzz/bin/styio_fuzz_parser tests/fuzz/corpus/parser \
  -runs=10000 -timeout=10 -max_len=65536 -seed=2 \
  -dict=tests/fuzz/styio.dict -use_value_profile=1
./build/fuzz/bin/styio_fuzz_ide_syntax tests/fuzz/corpus/ide_syntax -runs=10000
./build/fuzz/bin/styio_fuzz_ide_completion tests/fuzz/corpus/ide_completion -runs=10000
./build/fuzz/bin/styio_fuzz_ide_lsp_sync tests/fuzz/corpus/ide_lsp_sync -runs=10000
```

## Weekly Campaign

`Unka-Malloc/styio-nightly` 是唯一自动调度 fuzz 的仓库。`.github/workflows/nightly-fuzz.yml` 在周日 03:00 UTC 并行跑 lexer/parser，各 600 秒，并带 10 秒单样本看门狗、65536 字节上限、固定 seed、词典和值画像。语料先从 `tests/fuzz/corpus/<target>/` 复制，再叠加上一次演化缓存；失败样本仍走 `scripts/fuzz-regression-pack.sh`。

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
- `<target>/replay-options.json`：当时生效的 replay 参数
- `apply-corpus-backflow.sh`：将 seed 回流到 `tests/fuzz/corpus/`
