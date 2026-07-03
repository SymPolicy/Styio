# Styio Ecosystem CLI Contract Matrix

**Purpose:** 作为三仓协调镜像，冻结 `styio-nightly`、`styio-pafio`、`styio-view` 当前 active internal CLI contract 集合，并给跨仓文档一致性 gate 提供固定对照面。

**Last updated:** 2026-04-20

## 1. Rules

1. 每条 active internal CLI contract 都必须同时有：
   - coordinator mirror：本文件
   - owner contract：命令拥有方仓库的 SSOT
   - consumer handoff：下游仓库的对接文档
2. `pafio` 的 canonical machine-readable invocation spellings 固定为：
   - `pafio machine-info --json`
   - `pafio project-graph --manifest-path <path> --json`
   - `pafio tool status --manifest-path <path> --json`
   - `pafio --json build/run/test --manifest-path <path> ...`
   - `pafio --json fetch/vendor/pack/publish ...`
   - `pafio --json tool install/use/pin ...`
3. `styio` 的 compiler-side internal CLI contract 固定为：
   - `styio --machine-info=json`
   - `styio --compile-plan <path>`
   - `styio --source-build-info=json`
4. 任何 active internal CLI contract 变更，必须在同一 checkpoint 内同步更新三仓文档与 gate manifest。

## 2. `styio` -> `pafio` / `view`

### 2.1 `styio --machine-info=json`

Canonical form:

```text
styio --machine-info=json
```

当前跨仓必须保持一致的要点：

1. `active_integration_phase`
2. `supported_contract_versions`
3. `supported_adapter_modes`
4. `feature_flags`
5. `supported_contracts.compile_plan:[1]`
6. `supported_contracts.runtime_events:[1]`
7. `feature_flags.runtime_event_stream:true`

Owner / consumer docs:

1. `styio-pafio/docs/external/for-styio/Styio-External-Interface-Requirement-Spec.md`
2. `styio-view/docs/external/for-styio/Styio-Compile-Run-Contract.md`

### 2.2 `styio --compile-plan <path>`

Canonical form:

```text
styio --compile-plan <path>
```

当前跨仓必须保持一致的要点：

1. `build/check/run/test` 都走 compile-plan v1
2. 成功路径在 `build_root / artifact_dir / diag_dir` 内写出 receipt、产物、`diagnostics.jsonl` 和 `build_root/runtime-events.jsonl`
3. invalid plan / CLI conflict 也返回 machine-readable `CliError`
4. `receipt.json` 现在包含 `session_id` 与 `outputs.runtime_events_path`
5. `runtime-events.jsonl` 当前至少发布 `compile.* / run.* / thread.* / unit.* / unit.test.* / state.* / transition.fired / log.emitted / diagnostic.emitted`
6. `styio` 继续作为 receipt / diagnostics / runtime event artifact 的真相源

Owner / consumer docs:

1. `styio-pafio/docs/external/for-styio/Styio-External-Interface-Requirement-Spec.md`
2. `styio-view/docs/external/for-styio/Styio-Compile-Run-Contract.md`

### 2.3 `styio --source-build-info=json`

Canonical form:

```text
styio --source-build-info=json
```

当前跨仓必须保持一致的要点：

1. official source origin 固定为 `https://github.com/SymPolicy/Styio.git`
2. `stable` 和 `nightly` 通道映射到同名源码分支
3. official controlled source graph 当前冻结为 `compiler_core / std_symbols / runtime / macro_prelude`
4. 当前唯一官方 build mode 是 `minimal`
5. current helper entry is `scripts/source-build-minimal.sh`
6. compile-plan `profile.build_mode` 缺失时默认回落到 `minimal`，显式值当前也只允许 `minimal`
7. default symbol layer 的单一真相源当前是 `src/StyioParser/SymbolRegistry.cpp`
8. `--source-build-info=json` 只描述 source-build contract，不替代 binary 通道的 `--machine-info=json`

Owner / consumer docs:

1. `styio-nightly/docs/external/for-pafio/Styio-Nano-Pafio-Coordination.md`
2. `styio-pafio/docs/governance/Pafio-CLI-Contract.md`

### 2.4 `styio build <file_path> -o <artifact_name>`

Canonical form:

```text
styio build file_path -o artifact_name
```

当前必须保持一致的要点：

1. 输出产物是当前平台可直接执行的 native executable
2. `build` 阶段不执行 Styio entry program
3. 源文件先走现有 compile-plan `intent=build` 前端路径生成 LLVM IR
4. native executable 链接 Styio runtime helper surface，因此运行期错误仍按 Styio runtime error contract 返回
5. 该命令服务 benchmark `native-artifact` 路线，不替代 `--compile-plan <path>` 的生态构建合同

Owner / consumer docs:

1. `styio-benchmark/warm-process` 和 `styio-benchmark/polyglot` route 文档
2. `styio-nightly/tests/CMakeLists.txt` 的 `styio_build_native_executable_stdin_echo`

## 3. `pafio` -> `view`

### 3.1 `pafio machine-info --json`

Canonical form:

```text
pafio machine-info --json
```

当前跨仓必须保持一致的要点：

1. `active_integration_phase`
2. `supported_contracts.project_graph:[1]`
3. `supported_contracts.toolchain_state:[1]`
4. `supported_contracts.workflow_success_payloads:[1]`
5. `supported_adapter_modes:[cli]`
6. `feature_flags.runtime_event_payload:true`

Owner / consumer docs:

1. `styio-pafio/docs/governance/Pafio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Pafio-Toolchain-And-Registry-State.md`

### 3.2 `pafio project-graph --manifest-path <path> --json`

Canonical form:

```text
pafio project-graph --manifest-path <path> --json
```

当前跨仓必须保持一致的要点：

1. `project_graph v1`
2. `packages / dependencies / targets`
3. `toolchain / active_compiler / managed_toolchains`
4. `lock_state / vendor_state / notes`
5. `package_distribution`
6. `source_state`

Owner / consumer docs:

1. `styio-pafio/docs/governance/Pafio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Pafio-Project-Graph-Contract.md`

### 3.3 `pafio tool status --manifest-path <path> --json`

Canonical form:

```text
pafio tool status --manifest-path <path> --json
```

当前跨仓必须保持一致的要点：

1. `toolchain_state v1`
2. `toolchain`
3. `project_pin`
4. `active_compiler`
5. `current_compiler`
6. `managed_toolchains`
7. `notes`

Owner / consumer docs:

1. `styio-pafio/docs/governance/Pafio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Pafio-Toolchain-And-Registry-State.md`

### 3.4 `pafio --json build/run/test`

Canonical forms:

```text
pafio --json build --manifest-path <path> ...
pafio --json run --manifest-path <path> ...
pafio --json test --manifest-path <path> ...
```

当前跨仓必须保持一致的要点：

1. `workflow_success_payloads v1`
2. `build_root / artifact_dir / diag_dir`
3. `receipt_path`
4. parsed `receipt`
5. `diagnostics_path`
6. `runtime_events_path`
7. `runtime_session_id`
8. parsed `runtime_events`
9. captured `stdout / stderr`

Owner / consumer docs:

1. `styio-pafio/docs/governance/Pafio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Pafio-Workflow-Success-Payloads.md`

### 3.5 `pafio --json fetch/vendor/pack/publish`

Canonical forms:

```text
pafio --json fetch --manifest-path <path> ...
pafio --json vendor --manifest-path <path> ...
pafio --json pack --manifest-path <path> ...
pafio --json publish --manifest-path <path> --dry-run
pafio --json publish --manifest-path <path> --registry <path-or-url>
```

当前跨仓必须保持一致的要点：

1. supporting commands 成功时也必须写稳定 JSON object
2. success JSON 至少带 `command` 与 `message`
3. deploy/source-state 路径还必须给出 `archive_path`、`package` 或对应 command metadata
4. `source_state` 与 `package_distribution` 是 IDE deployment/source-management 的真相源

Owner / consumer docs:

1. `styio-pafio/docs/governance/Pafio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Pafio-Workflow-Success-Payloads.md`
3. `styio-view/docs/external/for-pafio/Pafio-Toolchain-And-Registry-State.md`

### 3.6 `pafio --json tool install/use/pin`

Canonical forms:

```text
pafio --json tool install --styio-bin <path>
pafio --json tool use --version <compiler-version> [--channel <channel>]
pafio --json tool pin --version <compiler-version> [--channel <channel>] --manifest-path <path>
pafio --json tool pin --clear --manifest-path <path>
```

当前跨仓必须保持一致的要点：

1. toolchain lifecycle 通过 success JSON 返回，不靠 stdout prose
2. project pin、managed installs、current compiler 统一回流到 `toolchain_state v1`
3. `styio-view` 只能触发 adapter，不再自己拼另一套命令语义

Owner / consumer docs:

1. `styio-pafio/docs/governance/Pafio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Pafio-Workflow-Success-Payloads.md`
3. `styio-view/docs/external/for-pafio/Pafio-Toolchain-And-Registry-State.md`
