# Styio Ecosystem CLI Contract Matrix

**Purpose:** 作为三仓协调镜像，冻结 `styio-nightly`、`styio-spio`、`styio-view` 当前 active internal CLI contract 集合，并给跨仓文档一致性 gate 提供固定对照面。

**Last updated:** 2026-06-28

## 前置条件

1. 并行: contract rows should be audited in parallel by owner command, consumer repository, payload field family, and negative-path evidence. Only shared payload vocabulary, canonical spellings, and gate ownership changes are serial coordinator merge gates.
2. 子智能体: sub-agents may check one contract family against one consumer repository, or one payload family across repositories, but one coordinator must merge the matrix update and gate evidence.
3. 基座: shared CLI payload, docs gate, workflow, or machine-readable contract substrate changes must land first through [Styio-Common-Foundation-Plan.md](./Styio-Common-Foundation-Plan.md).
4. Upstream dependency: ecosystem sequencing remains governed by [Styio-Ecosystem-Delivery-Master-Plan.md](./Styio-Ecosystem-Delivery-Master-Plan.md).

## Parallel Lane Map

| Lane | Parallel work allowed | Serial merge gate |
|------|-----------------------|-------------------|
| Owner-command rows | Audit each `styio`, `spio`, or `view` command family against its owner SSOT. | Canonical command spelling or payload vocabulary changes. |
| Consumer handoff rows | Check one consumer repo's docs against the matrix and record drift. | Matrix row changes that alter cross-repo expectations. |
| Payload fields | Verify diagnostics, receipts, runtime events, project graph, toolchain state, registry, and deploy payload fields independently. | Shared envelope/schema changes or compatibility promises. |
| Negative-path evidence | Discover missing malformed-input, dependency, publish, deploy, or execution failure tests independently. | New required gate policy or release-blocking matrix changes. |

## 1. Rules

1. 每条 active internal CLI contract 都必须同时有：
   - coordinator mirror：本文件
   - owner contract：命令拥有方仓库的 SSOT
   - consumer handoff：下游仓库的对接文档
2. `spio` 的 canonical machine-readable invocation spellings 固定为：
   - `spio machine-info --json`
   - `spio project-graph --manifest-path <path> --json`
   - `spio tool status --manifest-path <path> --json`
   - `spio --json build/run/test --manifest-path <path> ...`
   - `spio --json fetch/vendor/pack/publish ...`
   - `spio --json tool install/use/pin ...`
3. `styio` 的 compiler-side internal CLI contract 固定为：
   - `styio --machine-info=json`
   - `styio check --syntax --json --file <path>`
   - `styio --compile-plan <path>`
   - `styio --source-build-info=json`
4. 任何 active internal CLI contract 变更，必须在同一 checkpoint 内同步更新三仓文档与 gate manifest。

## 2. `styio` -> `spio` / `view`

### 2.1 `styio --machine-info=json`

Canonical form:

```text
styio --machine-info=json
```

当前跨仓必须保持一致的要点：

1. `active_integration_phase`
2. `supported_contracts` reports supported contract shapes rather than numbered implementation lanes
3. `supported_adapter_modes`
4. `feature_flags`
5. `supported_contracts.syntax_check:[syntax-json]`
6. `supported_contracts.compile_plan:[resolved-request]`
7. `supported_contracts.runtime_events:[jsonl]`
8. `feature_flags.syntax_check:true`
9. `feature_flags.runtime_event_stream:true`

Owner / consumer docs:

1. `styio-spio/docs/external/for-styio/Styio-External-Interface-Requirement-Spec.md`
2. `styio-view/docs/external/for-styio/Styio-Compile-Run-Contract.md`

### 2.2 `styio check --syntax --json --file <path>`

Canonical form:

```text
styio check --syntax --json --file <path>
```

当前跨仓必须保持一致的要点：

1. 只执行 lexing、parsing、AST construction。
2. 不执行 type checking、resource topology sema、lowering、codegen、runtime execution。
3. 输出单个 JSON result object，`contract` 固定为 `syntax-check`。
4. 成功返回 `0`，lexical error 返回 `2`，syntax/parse error 返回 `3`，CLI misuse 返回 `6`。
5. diagnostics 必须携带 `phase`、`code`、`line`、`column`、`offset`、`length`。
6. 该 contract 是通用语言服务入口，不绑定 `spio`、Vityo 或某个 IDE。

Owner / consumer docs:

1. [../external/SERVICES.md](../external/SERVICES.md)
2. `styio-view` / Vityo / third-party editor integrations can consume this before adopting full LSP.

### 2.3 `styio --compile-plan <path>`

Canonical form:

```text
styio --compile-plan <path>
```

当前跨仓必须保持一致的要点：

1. `build/check/run/test` 都走同一条 compile-plan resolved-request 入口
2. 成功路径在 `build_root / artifact_dir / diag_dir` 内写出 receipt、产物、`diagnostics.jsonl` 和 `build_root/runtime-events.jsonl`
3. invalid plan / CLI conflict 也返回 machine-readable `CliError`
4. `receipt.json` 现在包含 `session_id` 与 `outputs.runtime_events_path`
5. `runtime-events.jsonl` 当前至少发布 `compile.* / run.* / thread.* / unit.* / unit.test.* / state.* / transition.fired / log.emitted / diagnostic.emitted`
6. `styio` 继续作为 receipt / diagnostics / runtime event artifact 的真相源

Owner / consumer docs:

1. `styio-spio/docs/external/for-styio/Styio-External-Interface-Requirement-Spec.md`
2. `styio-view/docs/external/for-styio/Styio-Compile-Run-Contract.md`

### 2.4 `styio --source-build-info=json`

Canonical form:

```text
styio --source-build-info=json
```

当前跨仓必须保持一致的要点：

1. official source origin 固定为 `https://github.com/eBioRing/Styio.git`
2. `stable` 和 `nightly` 通道映射到同名源码分支
3. official controlled source graph 当前冻结为 `compiler_core / std_symbols / runtime / services / macro_prelude`
4. 当前唯一官方 build mode 是 `minimal`
5. current helper entry is `scripts/source-build-minimal.sh`
6. compile-plan `profile.build_mode` 缺失时默认回落到 `minimal`，显式值当前也只允许 `minimal`
7. default symbol layer 的单一真相源当前是 `src/StyioParser/SymbolRegistry.cpp`
8. `--source-build-info=json` 只描述 source-build contract，不替代 binary 通道的 `--machine-info=json`

Owner / consumer docs:

1. `styio-nightly/docs/external/for-pafio/Styio-Nano-Pafio-Coordination.md`
2. `styio-spio/docs/governance/Spio-CLI-Contract.md`

### 2.5 `styio build <file_path> -o <artifact_name>`

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

## 3. `spio` -> `view`

### 3.1 `spio machine-info --json`

Canonical form:

```text
spio machine-info --json
```

当前跨仓必须保持一致的要点：

1. `active_integration_phase`
2. `supported_contracts.project_graph:[package-workspace-shape]`
3. `supported_contracts.toolchain_state:[compiler-toolchain-shape]`
4. `supported_contracts.workflow_success_payloads:[execution-result-shape]`
5. `supported_adapter_modes:[cli]`
6. `feature_flags.runtime_event_payload:true`

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Spio-Toolchain-And-Registry-State.md`

### 3.2 `spio project-graph --manifest-path <path> --json`

Canonical form:

```text
spio project-graph --manifest-path <path> --json
```

当前跨仓必须保持一致的要点：

1. `project_graph package-workspace-shape`
2. `packages / dependencies / targets`
3. `toolchain / active_compiler / managed_toolchains`
4. `lock_state / vendor_state / notes`
5. `package_distribution`
6. `source_state`

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Spio-Project-Graph-Contract.md`

### 3.3 `spio tool status --manifest-path <path> --json`

Canonical form:

```text
spio tool status --manifest-path <path> --json
```

当前跨仓必须保持一致的要点：

1. `toolchain_state compiler-toolchain-shape`
2. `toolchain`
3. `project_pin`
4. `active_compiler`
5. `current_compiler`
6. `managed_toolchains`
7. `notes`

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Spio-Toolchain-And-Registry-State.md`

### 3.4 `spio --json build/run/test`

Canonical forms:

```text
spio --json build --manifest-path <path> ...
spio --json run --manifest-path <path> ...
spio --json test --manifest-path <path> ...
```

当前跨仓必须保持一致的要点：

1. `workflow_success_payloads execution-result-shape`
2. `build_root / artifact_dir / diag_dir`
3. `receipt_path`
4. parsed `receipt`
5. `diagnostics_path`
6. `runtime_events_path`
7. `runtime_session_id`
8. parsed `runtime_events`
9. captured `stdout / stderr`

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Spio-Workflow-Success-Payloads.md`

### 3.5 `spio --json fetch/vendor/pack/publish`

Canonical forms:

```text
spio --json fetch --manifest-path <path> ...
spio --json vendor --manifest-path <path> ...
spio --json pack --manifest-path <path> ...
spio --json publish --manifest-path <path> --dry-run
spio --json publish --manifest-path <path> --registry <path-or-url>
```

当前跨仓必须保持一致的要点：

1. supporting commands 成功时也必须写稳定 JSON object
2. success JSON 至少带 `command` 与 `message`
3. deploy/source-state 路径还必须给出 `archive_path`、`package` 或对应 command metadata
4. `source_state` 与 `package_distribution` 是 IDE deployment/source-management 的真相源

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Spio-Workflow-Success-Payloads.md`
3. `styio-view/docs/external/for-pafio/Spio-Toolchain-And-Registry-State.md`

### 3.6 `spio --json tool install/use/pin`

Canonical forms:

```text
spio --json tool install --styio-bin <path>
spio --json tool use --version <compiler-version> [--channel <channel>]
spio --json tool pin --version <compiler-version> [--channel <channel>] --manifest-path <path>
spio --json tool pin --clear --manifest-path <path>
```

当前跨仓必须保持一致的要点：

1. toolchain lifecycle 通过 success JSON 返回，不靠 stdout prose
2. project pin、managed installs、current compiler 统一回流到 `toolchain_state compiler-toolchain-shape`
3. `styio-view` 只能触发 adapter，不再自己拼另一套命令语义

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/external/for-pafio/Spio-Workflow-Success-Payloads.md`
3. `styio-view/docs/external/for-pafio/Spio-Toolchain-And-Registry-State.md`

## 验收条件

1. Every active internal CLI contract row names the canonical invocation and owner or consumer docs.
2. Cross-repository mirrors are updated in the same checkpoint when a contract shape changes.
3. `python3 scripts/ecosystem-cli-doc-gate.py` passes for local contract consistency.
4. Any shared payload or gate substrate change is first accepted through the common foundation plan.
