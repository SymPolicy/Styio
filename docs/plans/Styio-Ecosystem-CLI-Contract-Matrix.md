# Styio Ecosystem CLI Contract Matrix

**Purpose:** 作为三仓协调镜像，冻结 `styio-nightly`、`styio-spio`、`styio-view` 当前 active internal CLI contract 集合，并给跨仓文档一致性 gate 提供固定对照面。

**Last updated:** 2026-04-17

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
   - `styio --compile-plan <path>`
4. 任何 active internal CLI contract 变更，必须在同一 checkpoint 内同步更新三仓文档与 gate manifest。

## 2. `styio` -> `spio` / `view`

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

Owner / consumer docs:

1. `styio-spio/docs/styio/Styio-External-Interface-Requirement-Spec.md`
2. `styio-view/docs/for-styio/Styio-Compile-Run-Contract.md`

### 2.2 `styio --compile-plan <path>`

Canonical form:

```text
styio --compile-plan <path>
```

当前跨仓必须保持一致的要点：

1. `build/check/run/test` 都走 compile-plan v1
2. 成功路径在 `build_root / artifact_dir / diag_dir` 内写出 receipt、产物和 `diagnostics.jsonl`
3. invalid plan / CLI conflict 也返回 machine-readable `CliError`
4. `styio` 继续作为 receipt / diagnostics 的真相源

Owner / consumer docs:

1. `styio-spio/docs/styio/Styio-External-Interface-Requirement-Spec.md`
2. `styio-view/docs/for-styio/Styio-Compile-Run-Contract.md`

## 3. `spio` -> `view`

### 3.1 `spio machine-info --json`

Canonical form:

```text
spio machine-info --json
```

当前跨仓必须保持一致的要点：

1. `active_integration_phase`
2. `supported_contracts.project_graph:[1]`
3. `supported_contracts.toolchain_state:[1]`
4. `supported_contracts.workflow_success_payloads:[1]`
5. `supported_adapter_modes:[cli]`

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/for-spio/Spio-Toolchain-And-Registry-State.md`

### 3.2 `spio project-graph --manifest-path <path> --json`

Canonical form:

```text
spio project-graph --manifest-path <path> --json
```

当前跨仓必须保持一致的要点：

1. `project_graph v1`
2. `packages / dependencies / targets`
3. `toolchain / active_compiler / managed_toolchains`
4. `lock_state / vendor_state / notes`
5. `package_distribution`
6. `source_state`

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/for-spio/Spio-Project-Graph-Contract.md`

### 3.3 `spio tool status --manifest-path <path> --json`

Canonical form:

```text
spio tool status --manifest-path <path> --json
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

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/for-spio/Spio-Toolchain-And-Registry-State.md`

### 3.4 `spio --json build/run/test`

Canonical forms:

```text
spio --json build --manifest-path <path> ...
spio --json run --manifest-path <path> ...
spio --json test --manifest-path <path> ...
```

当前跨仓必须保持一致的要点：

1. `workflow_success_payloads v1`
2. `build_root / artifact_dir / diag_dir`
3. `receipt_path`
4. parsed `receipt`
5. `diagnostics_path`
6. captured `stdout / stderr`

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/for-spio/Spio-Workflow-Success-Payloads.md`

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
2. `styio-view/docs/for-spio/Spio-Workflow-Success-Payloads.md`
3. `styio-view/docs/for-spio/Spio-Toolchain-And-Registry-State.md`

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
2. project pin、managed installs、current compiler 统一回流到 `toolchain_state v1`
3. `styio-view` 只能触发 adapter，不再自己拼另一套命令语义

Owner / consumer docs:

1. `styio-spio/docs/governance/Spio-CLI-Contract.md`
2. `styio-view/docs/for-spio/Spio-Workflow-Success-Payloads.md`
3. `styio-view/docs/for-spio/Spio-Toolchain-And-Registry-State.md`
