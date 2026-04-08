# Styio 测试目录（按功能域）

**文档作用：** 将 **里程碑集成测试** 按功能域映射到 **输入 `.styio`、golden/副作用路径与 `ctest` 命令**；权威自动化入口见 `tests/CMakeLists.txt`。维护规则见 [`../../specs/DOCUMENTATION-POLICY.md`](../../specs/DOCUMENTATION-POLICY.md)。

**Last updated:** 2026-04-08（定位并修复 `ParamAST` 空类型崩溃；恢复 hash `>>` iterator 定义；补 `?=` forward chain 稳定拒绝回归；补 `match` 非整型 scrutinee TypeError 防崩溃回归；完成 C.8 句柄表接管并补 Safety 回归；新增非法句柄 Soak 与 nightly sanitizer 门禁；统一 runtime text/jsonl 诊断出口并冻结 runtime subcode 回归；修复 codegen 复合赋值与 stream zip 不支持来源误报 RuntimeError 分类；活跃流水线移除 `Styio.NotImplemented` 文案；修复 `?=` 无默认分支未初始化指针崩溃并补回归；补 `?= {}` 拒绝契约与 malformed 前缀 parse 防崩溃回归；fuzz smoke 接入 clang-18 链路并稳定通过；nightly fuzz 新增 case pack 回流与 `fuzz_regression_pack_smoke`，且 `styio_fuzz_parser` 改为同一输入覆盖 legacy/nightly 双引擎；ParserContext 新增空 token EOF 降级与越界前移钳制回归；新增 `parse_path` 单字符路径与 `peak_operator` EOF 越界防护回归；补 `CasesAST/MatchCasesAST`、`PrintAST/StateRefAST/HistoryProbeAST`、`SeriesIntrinsicAST`、`StateDeclAST`、`TypeTupleAST` 与 `CheckEqualAST` 所有权析构回归并完成 RAII 收口；修复单参数 state helper（直返 `StateDecl`）在 pulse 体中调用时参数替换失效问题，并补 `1/3/6` 回归；补 state helper 内联 `MatchCases/Cases` 与 `InfiniteAST` 克隆覆盖回归（输出 `10/12/15`、`0/0`）；新增 `StateInlineHelperProgramLoop`、`StateInlineMatchCasesProgramLoop`、`StateInlineInfiniteProgramLoop` 与对应 `soak_deep` 长跑门禁；将 parser 默认引擎切换到 `nightly` 并冻结默认主引擎契约回归；nightly parser 新增 block/control、`?={...}`、资源 postfix、`@[...]` / `@file...` 起始、`[ ... ] >> ...` list-start iterator 子集与 plain state ref `$name`，并在 shadow artifact `detail` 暴露并收敛 route stats 到 M7 零 fallback/零 internal bridge；M1/M2/M7 shadow gates 已正式纳入 CTest 与 `checkpoint-health`，M5 则通过 `shadow-expected-nonzero.txt` 接入 dual-zero gate；five-layer pipeline 的 parser 层也已对齐 nightly，并新增 `p07_instant_pull` case；新增 `nightly_internal_legacy_bridges` 指标并先收缩 iterator / match-cases / infinite-loop / forward body 的内部 legacy bridge；恢复默认 nightly 对 `M4` wave 语法的安全回退，并修正 `true >> @stdout` 的 bool literal 误判）

**批量自动化（所有里程碑集成用例）：**

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build -L milestone
```

**改完代码后的修改 + 测试报告（本地 Markdown）：** 在项目根执行 `./scripts/dev-report.sh`（可选 `--no-build`、`--note "说明"`）。每次生成带时间戳的 `reports/dev-report-*.md`，并更新同目录 `LATEST.md`（目录默认在 `.gitignore`，需要留存可拷到 `docs/history/`）。五层流水线见 [`FIVE-LAYER-PIPELINE.md`](./FIVE-LAYER-PIPELINE.md)。

**约定：** 除非另表说明，**输入** 为 `tests/milestones/<mN>/<name>.styio`，**期望 stdout** 为 `tests/milestones/<mN>/expected/<name>.out`；CTest 名为 `<mN>_<name>`（例如 `m1_t01_int_arith`）。比对方式：`styio --file <input> | cmp -s - <oracle>`。

**辅助数据：** `tests/milestones/m5/data/`、`tests/milestones/m7/data/` 中的文件由对应 `.styio` 通过 `@file` 等读取，不单独注册 CTest。

---

## 1. 基础：表达式、打印、绑定（M1）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m1_t01_int_arith` | `tests/milestones/m1/t01_int_arith.styio` | `tests/milestones/m1/expected/t01_int_arith.out` | `ctest --test-dir build -R '^m1_t01_int_arith$'` |
| `m1_t02_precedence` | `tests/milestones/m1/t02_precedence.styio` | `tests/milestones/m1/expected/t02_precedence.out` | `ctest --test-dir build -R '^m1_t02_precedence$'` |
| `m1_t03_parens` | `tests/milestones/m1/t03_parens.styio` | `tests/milestones/m1/expected/t03_parens.out` | `ctest --test-dir build -R '^m1_t03_parens$'` |
| `m1_t04_float` | `tests/milestones/m1/t04_float.styio` | `tests/milestones/m1/expected/t04_float.out` | `ctest --test-dir build -R '^m1_t04_float$'` |
| `m1_t05_mixed` | `tests/milestones/m1/t05_mixed.styio` | `tests/milestones/m1/expected/t05_mixed.out` | `ctest --test-dir build -R '^m1_t05_mixed$'` |
| `m1_t06_binding` | `tests/milestones/m1/t06_binding.styio` | `tests/milestones/m1/expected/t06_binding.out` | `ctest --test-dir build -R '^m1_t06_binding$'` |
| `m1_t07_typed_bind` | `tests/milestones/m1/t07_typed_bind.styio` | `tests/milestones/m1/expected/t07_typed_bind.out` | `ctest --test-dir build -R '^m1_t07_typed_bind$'` |
| `m1_t08_string` | `tests/milestones/m1/t08_string.styio` | `tests/milestones/m1/expected/t08_string.out` | `ctest --test-dir build -R '^m1_t08_string$'` |
| `m1_t09_multi_print` | `tests/milestones/m1/t09_multi_print.styio` | `tests/milestones/m1/expected/t09_multi_print.out` | `ctest --test-dir build -R '^m1_t09_multi_print$'` |
| `m1_t10_modulo` | `tests/milestones/m1/t10_modulo.styio` | `tests/milestones/m1/expected/t10_modulo.out` | `ctest --test-dir build -R '^m1_t10_modulo$'` |
| `m1_t11_compare` | `tests/milestones/m1/t11_compare.styio` | `tests/milestones/m1/expected/t11_compare.out` | `ctest --test-dir build -R '^m1_t11_compare$'` |
| `m1_t12_logic` | `tests/milestones/m1/t12_logic.styio` | `tests/milestones/m1/expected/t12_logic.out` | `ctest --test-dir build -R '^m1_t12_logic$'` |
| `m1_t13_negative` | `tests/milestones/m1/t13_negative.styio` | `tests/milestones/m1/expected/t13_negative.out` | `ctest --test-dir build -R '^m1_t13_negative$'` |
| `m1_t14_compound` | `tests/milestones/m1/t14_compound.styio` | `tests/milestones/m1/expected/t14_compound.out` | `ctest --test-dir build -R '^m1_t14_compound$'` |
| `m1_t15_int_div` | `tests/milestones/m1/t15_int_div.styio` | `tests/milestones/m1/expected/t15_int_div.out` | `ctest --test-dir build -R '^m1_t15_int_div$'` |
| `m1_t16_float_div` | `tests/milestones/m1/t16_float_div.styio` | `tests/milestones/m1/expected/t16_float_div.out` | `ctest --test-dir build -R '^m1_t16_float_div$'` |
| `m1_t17_power` | `tests/milestones/m1/t17_power.styio` | `tests/milestones/m1/expected/t17_power.out` | `ctest --test-dir build -R '^m1_t17_power$'` |
| `m1_t18_bool` | `tests/milestones/m1/t18_bool.styio` | `tests/milestones/m1/expected/t18_bool.out` | `ctest --test-dir build -R '^m1_t18_bool$'` |
| `m1_t19_chain_bind` | `tests/milestones/m1/t19_chain_bind.styio` | `tests/milestones/m1/expected/t19_chain_bind.out` | `ctest --test-dir build -R '^m1_t19_chain_bind$'` |
| `m1_t20_combined` | `tests/milestones/m1/t20_combined.styio` | `tests/milestones/m1/expected/t20_combined.out` | `ctest --test-dir build -R '^m1_t20_combined$'` |

**整组：** `ctest --test-dir build -L m1`

---

## 2. 函数：定义、调用、返回（M2）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m2_t01_simple_func` | `tests/milestones/m2/t01_simple_func.styio` | `tests/milestones/m2/expected/t01_simple_func.out` | `ctest --test-dir build -R '^m2_t01_simple_func$'` |
| `m2_t02_typed_return` | `tests/milestones/m2/t02_typed_return.styio` | `tests/milestones/m2/expected/t02_typed_return.out` | `ctest --test-dir build -R '^m2_t02_typed_return$'` |
| `m2_t03_block_body` | `tests/milestones/m2/t03_block_body.styio` | `tests/milestones/m2/expected/t03_block_body.out` | `ctest --test-dir build -R '^m2_t03_block_body$'` |
| `m2_t04_func_chain` | `tests/milestones/m2/t04_func_chain.styio` | `tests/milestones/m2/expected/t04_func_chain.out` | `ctest --test-dir build -R '^m2_t04_func_chain$'` |
| `m2_t05_float_func` | `tests/milestones/m2/t05_float_func.styio` | `tests/milestones/m2/expected/t05_float_func.out` | `ctest --test-dir build -R '^m2_t05_float_func$'` |
| `m2_t06_multi_stmt` | `tests/milestones/m2/t06_multi_stmt.styio` | `tests/milestones/m2/expected/t06_multi_stmt.out` | `ctest --test-dir build -R '^m2_t06_multi_stmt$'` |
| `m2_t07_no_params` | `tests/milestones/m2/t07_no_params.styio` | `tests/milestones/m2/expected/t07_no_params.out` | `ctest --test-dir build -R '^m2_t07_no_params$'` |
| `m2_t09_bind_call` | `tests/milestones/m2/t09_bind_call.styio` | `tests/milestones/m2/expected/t09_bind_call.out` | `ctest --test-dir build -R '^m2_t09_bind_call$'` |
| `m2_t10_nested` | `tests/milestones/m2/t10_nested.styio` | `tests/milestones/m2/expected/t10_nested.out` | `ctest --test-dir build -R '^m2_t10_nested$'` |
| `m2_t11_hash_assign_and_arrow` | `tests/milestones/m2/t11_hash_assign_and_arrow.styio` | `tests/milestones/m2/expected/t11_hash_assign_and_arrow.out` | `ctest --test-dir build -R '^m2_t11_hash_assign_and_arrow$'` |

**整组：** `ctest --test-dir build -L m2`（仓库中无 `t08`，与里程碑编号不连续属预期。）

---

## 3. 控制流：match、循环、break、continue（M3）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m3_t01_match` | `tests/milestones/m3/t01_match.styio` | `tests/milestones/m3/expected/t01_match.out` | `ctest --test-dir build -R '^m3_t01_match$'` |
| `m3_t02_match_expr` | `tests/milestones/m3/t02_match_expr.styio` | `tests/milestones/m3/expected/t02_match_expr.out` | `ctest --test-dir build -R '^m3_t02_match_expr$'` |
| `m3_t03_match_default` | `tests/milestones/m3/t03_match_default.styio` | `tests/milestones/m3/expected/t03_match_default.out` | `ctest --test-dir build -R '^m3_t03_match_default$'` |
| `m3_t04_factorial` | `tests/milestones/m3/t04_factorial.styio` | `tests/milestones/m3/expected/t04_factorial.out` | `ctest --test-dir build -R '^m3_t04_factorial$'` |
| `m3_t05_for_each` | `tests/milestones/m3/t05_for_each.styio` | `tests/milestones/m3/expected/t05_for_each.out` | `ctest --test-dir build -R '^m3_t05_for_each$'` |
| `m3_t06_inf_break` | `tests/milestones/m3/t06_inf_break.styio` | `tests/milestones/m3/expected/t06_inf_break.out` | `ctest --test-dir build -R '^m3_t06_inf_break$'` |
| `m3_t07_while` | `tests/milestones/m3/t07_while.styio` | `tests/milestones/m3/expected/t07_while.out` | `ctest --test-dir build -R '^m3_t07_while$'` |
| `m3_t08_multi_break` | `tests/milestones/m3/t08_multi_break.styio` | `tests/milestones/m3/expected/t08_multi_break.out` | `ctest --test-dir build -R '^m3_t08_multi_break$'` |
| `m3_t09_continue` | `tests/milestones/m3/t09_continue.styio` | `tests/milestones/m3/expected/t09_continue.out` | `ctest --test-dir build -R '^m3_t09_continue$'` |
| `m3_t10_fizzbuzz` | `tests/milestones/m3/t10_fizzbuzz.styio` | `tests/milestones/m3/expected/t10_fizzbuzz.out` | `ctest --test-dir build -R '^m3_t10_fizzbuzz$'` |

**整组：** `ctest --test-dir build -L m3`

---

## 4. 波算子、@ 传播、fallback（M4）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m4_t01_wave_merge` | `tests/milestones/m4/t01_wave_merge.styio` | `tests/milestones/m4/expected/t01_wave_merge.out` | `ctest --test-dir build -R '^m4_t01_wave_merge$'` |
| `m4_t16_wave_merge_question_cond` | `tests/milestones/m4/t16_wave_merge_question_cond.styio` | `tests/milestones/m4/expected/t16_wave_merge_question_cond.out` | `ctest --test-dir build -R '^m4_t16_wave_merge_question_cond$'` |
| `m4_t02_wave_false` | `tests/milestones/m4/t02_wave_false.styio` | `tests/milestones/m4/expected/t02_wave_false.out` | `ctest --test-dir build -R '^m4_t02_wave_false$'` |
| `m4_t03_wave_dispatch` | `tests/milestones/m4/t03_wave_dispatch.styio` | `tests/milestones/m4/expected/t03_wave_dispatch.out` | `ctest --test-dir build -R '^m4_t03_wave_dispatch$'` |
| `m4_t04_dispatch_void` | `tests/milestones/m4/t04_dispatch_void.styio` | `tests/milestones/m4/expected/t04_dispatch_void.out` | `ctest --test-dir build -R '^m4_t04_dispatch_void$'` |
| `m4_t05_undefined` | `tests/milestones/m4/t05_undefined.styio` | `tests/milestones/m4/expected/t05_undefined.out` | `ctest --test-dir build -R '^m4_t05_undefined$'` |
| `m4_t06_at_propagate` | `tests/milestones/m4/t06_at_propagate.styio` | `tests/milestones/m4/expected/t06_at_propagate.out` | `ctest --test-dir build -R '^m4_t06_at_propagate$'` |
| `m4_t07_at_logic` | `tests/milestones/m4/t07_at_logic.styio` | `tests/milestones/m4/expected/t07_at_logic.out` | `ctest --test-dir build -R '^m4_t07_at_logic$'` |
| `m4_t08_fallback` | `tests/milestones/m4/t08_fallback.styio` | `tests/milestones/m4/expected/t08_fallback.out` | `ctest --test-dir build -R '^m4_t08_fallback$'` |
| `m4_t09_fallback_noop` | `tests/milestones/m4/t09_fallback_noop.styio` | `tests/milestones/m4/expected/t09_fallback_noop.out` | `ctest --test-dir build -R '^m4_t09_fallback_noop$'` |
| `m4_t10_guard` | `tests/milestones/m4/t10_guard.styio` | `tests/milestones/m4/expected/t10_guard.out` | `ctest --test-dir build -R '^m4_t10_guard$'` |
| `m4_t11_guard_pass` | `tests/milestones/m4/t11_guard_pass.styio` | `tests/milestones/m4/expected/t11_guard_pass.out` | `ctest --test-dir build -R '^m4_t11_guard_pass$'` |
| `m4_t12_eq_probe` | `tests/milestones/m4/t12_eq_probe.styio` | `tests/milestones/m4/expected/t12_eq_probe.out` | `ctest --test-dir build -R '^m4_t12_eq_probe$'` |
| `m4_t13_chain` | `tests/milestones/m4/t13_chain.styio` | `tests/milestones/m4/expected/t13_chain.out` | `ctest --test-dir build -R '^m4_t13_chain$'` |
| `m4_t14_func_gate` | `tests/milestones/m4/t14_func_gate.styio` | `tests/milestones/m4/expected/t14_func_gate.out` | `ctest --test-dir build -R '^m4_t14_func_gate$'` |
| `m4_t15_deep_propagate` | `tests/milestones/m4/t15_deep_propagate.styio` | `tests/milestones/m4/expected/t15_deep_propagate.out` | `ctest --test-dir build -R '^m4_t15_deep_propagate$'` |

**整组：** `ctest --test-dir build -L m4`

---

## 5. 资源与 I/O（M5）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m5_t01_read_file` | `tests/milestones/m5/t01_read_file.styio` | stdout vs `tests/milestones/m5/expected/t01_read_file.out` | `ctest --test-dir build -R '^m5_t01_read_file$'` |
| `m5_t02_write_file` | `tests/milestones/m5/t02_write_file.styio` | 临时文件 `/tmp/styio_out.txt` 内容等于 `Hello from Styio`（UTF-8 无换行） | `ctest --test-dir build -R '^m5_t02_write_file$'` |
| `m5_t04_raii` | `tests/milestones/m5/t04_raii.styio` | stdout vs `tests/milestones/m5/expected/t04_raii.out` | `ctest --test-dir build -R '^m5_t04_raii$'` |
| `m5_t05_auto_detect` | `tests/milestones/m5/t05_auto_detect.styio` | stdout vs `tests/milestones/m5/expected/t05_auto_detect.out` | `ctest --test-dir build -R '^m5_t05_auto_detect$'` |
| `m5_t06_fail_fast` | `tests/milestones/m5/t06_fail_fast.styio` | **进程退出码非 0**（stderr 可非空） | `ctest --test-dir build -R '^m5_t06_fail_fast$'` |
| `m5_t07_redirect` | `tests/milestones/m5/t07_redirect.styio` | 临时文件 `/tmp/styio_val.txt` 内容等于 `42` | `ctest --test-dir build -R '^m5_t07_redirect$'` |
| `m5_t08_pipe_func` | `tests/milestones/m5/t08_pipe_func.styio` | stdout vs `tests/milestones/m5/expected/t08_pipe_func.out` | `ctest --test-dir build -R '^m5_t08_pipe_func$'` |

**整组：** `ctest --test-dir build -L m5`

---

## 6. 状态容器与流上算子（M6）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m6_t01_scan` | `tests/milestones/m6/t01_scan.styio` | `tests/milestones/m6/expected/t01_scan.out` | `ctest --test-dir build -R '^m6_t01_scan$'` |
| `m6_t02_running_max` | `tests/milestones/m6/t02_running_max.styio` | `tests/milestones/m6/expected/t02_running_max.out` | `ctest --test-dir build -R '^m6_t02_running_max$'` |
| `m6_t03_window_avg` | `tests/milestones/m6/t03_window_avg.styio` | `tests/milestones/m6/expected/t03_window_avg.out` | `ctest --test-dir build -R '^m6_t03_window_avg$'` |
| `m6_t04_history` | `tests/milestones/m6/t04_history.styio` | `tests/milestones/m6/expected/t04_history.out` | `ctest --test-dir build -R '^m6_t04_history$'` |
| `m6_t05_frame_lock` | `tests/milestones/m6/t05_frame_lock.styio` | `tests/milestones/m6/expected/t05_frame_lock.out` | `ctest --test-dir build -R '^m6_t05_frame_lock$'` |
| `m6_t06_cold_start` | `tests/milestones/m6/t06_cold_start.styio` | `tests/milestones/m6/expected/t06_cold_start.out` | `ctest --test-dir build -R '^m6_t06_cold_start$'` |

**整组：** `ctest --test-dir build -L m6`

**Gap：** `docs/milestones/2026-03-29/M6-StateAndStreams.md` 中若列出 `t07`–`t10` 等用例，本仓库 **尚未** 提供对应 `tests/milestones/m6/t07*.styio`…；补齐后需同步更新本表与 `tests/CMakeLists.txt`。

---

## 7. 多流 zip、快照、驱动（M7）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m7_t01_zip_collections` | `tests/milestones/m7/t01_zip_collections.styio` | stdout vs `tests/milestones/m7/expected/t01_zip_collections.out` | `ctest --test-dir build -R '^m7_t01_zip_collections$'` |
| `m7_t02_zip_unequal` | `tests/milestones/m7/t02_zip_unequal.styio` | stdout vs `tests/milestones/m7/expected/t02_zip_unequal.out` | `ctest --test-dir build -R '^m7_t02_zip_unequal$'` |
| `m7_t03_snapshot` | `tests/milestones/m7/t03_snapshot.styio` | stdout vs `tests/milestones/m7/expected/t03_snapshot.out` | `ctest --test-dir build -R '^m7_t03_snapshot$'` |
| `m7_t04_instant_pull` | `tests/milestones/m7/t04_instant_pull.styio` | stdout vs `tests/milestones/m7/expected/t04_instant_pull.out` | `ctest --test-dir build -R '^m7_t04_instant_pull$'` |
| `m7_t05_zip_files` | `tests/milestones/m7/t05_zip_files.styio` | stdout vs `tests/milestones/m7/expected/t05_zip_files.out`；数据见 `tests/milestones/m7/data/` | `ctest --test-dir build -R '^m7_t05_zip_files$'` |
| `m7_t06_snapshot_lock` | `tests/milestones/m7/t06_snapshot_lock.styio` | stdout vs `tests/milestones/m7/expected/t06_snapshot_lock.out` | `ctest --test-dir build -R '^m7_t06_snapshot_lock$'` |
| `m7_t07_singleton` | `tests/milestones/m7/t07_singleton.styio` | stdout vs `tests/milestones/m7/expected/t07_singleton.out` | `ctest --test-dir build -R '^m7_t07_singleton$'` |
| `m7_t08_arbitrage` | `tests/milestones/m7/t08_arbitrage.styio` | stdout vs `tests/milestones/m7/expected/t08_arbitrage.out`；数据见 `tests/milestones/m7/data/` | `ctest --test-dir build -R '^m7_t08_arbitrage$'` |
| `m7_t09_snapshot_accum` | `tests/milestones/m7/t09_snapshot_accum.styio` | stdout vs `tests/milestones/m7/expected/t09_snapshot_accum.out` | `ctest --test-dir build -R '^m7_t09_snapshot_accum$'` |
| `m7_t10_full_pipeline` | `tests/milestones/m7/t10_full_pipeline.styio` | 临时文件 `/tmp/styio_doubled.txt` 与 `tests/milestones/m7/expected/t10_full_pipeline.out` **字节一致**（`cmp -s`） | `ctest --test-dir build -R '^m7_t10_full_pipeline$'` |

**整组：** `ctest --test-dir build -L m7`

---

## 8. Topology v2 增量（M8）

| CTest | Input | Output / Oracle | Automation |
|-------|-------|-----------------|------------|
| `m8_t01_bounded_final_bind` | `tests/milestones/m8/t01_bounded_final_bind.styio` | `tests/milestones/m8/expected/t01_bounded_final_bind.out` | `ctest --test-dir build -R '^m8_t01_bounded_final_bind$'` |
| `m8_t02_bounded_read` | `tests/milestones/m8/t02_bounded_read.styio` | `tests/milestones/m8/expected/t02_bounded_read.out` | `ctest --test-dir build -R '^m8_t02_bounded_read$'` |
| `m8_t14_flex_other_var_after_final_ok` | `tests/milestones/m8/t14_flex_other_var_after_final_ok.styio` | `tests/milestones/m8/expected/t14_flex_other_var_after_final_ok.out` | `ctest --test-dir build -R '^m8_t14'` |

**Final 后禁止同名 Flex（语义失败用例，无 golden）：** `tests/milestones/m8/e01`–`e10`，CTest 名 `m8_err_e01_…` … `m8_err_e10_…`（标签 **`m8_semantic`**）。`ctest --test-dir build -L m8_semantic`。

**说明：** `[|n|]` 在 **final bind** 下生成 **`[n x i64]` + head**；**已对 `x` 使用 `:=` 后再写 `x = …`** 在 **typeInfer** 报错；`#` 形参环语义仍不完整，见 [`../../plans/BoundedRing-Codegen-Adjustment.md`](../../plans/BoundedRing-Codegen-Adjustment.md)。

**整组：** `ctest --test-dir build -L m8`

---

## 9. C++ 单元测试（GoogleTest）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_test` | `tests/styio_test.cpp`：`StyioFiveLayerPipeline`、`StyioParserEngine`、`StyioDiagnostics`（five-layer pipeline 的 L2 parser 已固定走 nightly，当前 cases 为 `p01_print_add`、`p02_simple_func`、`p03_write_file`、`p04_read_file`、`p05_snapshot_accum`、`p06_zip_files`、`p07_instant_pull`、`p08_redirect_file`、`p09_full_pipeline`、`p10_auto_detect_read`、`p11_pipe_func`、`p12_stdin_echo`、`p13_stdin_transform`、`p14_stdin_pull`、`p15_stdin_mixed_output`；其中 `p03_write_file` 额外断言 `/tmp/styio_pipeline_out.txt` 的副作用文件内容，`p04_read_file` 会在运行前准备 `/tmp/styio_pipeline_in.txt` 以覆盖文件读取 + iterator body，`p05_snapshot_accum` 会在运行前准备 `/tmp/styio_pipeline_factor.txt` 以覆盖 snapshot decl + state ref + iterator block + compound accumulation，`p06_zip_files` 会在运行前准备 `/tmp/styio_zip_a.txt` 和 `/tmp/styio_zip_b.txt` 以覆盖 file iterator + stream zip + arithmetic print，`p07_instant_pull` 会在运行前准备 `/tmp/styio_pull_value.txt` 以覆盖表达式级 `instant pull`，`p08_redirect_file` 则会断言 `/tmp/styio_pipeline_redirect.txt` 的 redirect 副作用内容，`p09_full_pipeline` 则会同时准备 `/tmp/styio_pipeline_stream_input.txt` 与断言 `/tmp/styio_pipeline_stream_output.txt`，覆盖 file iterator + bind + resource write 的组合链路，`p10_auto_detect_read` 会在运行前准备 `/tmp/styio_pipeline_auto_input.txt`，覆盖 `@{...}` auto-detect 读取 + iterator body，`p11_pipe_func` 会在运行前准备 `/tmp/styio_pipeline_numbers.txt`，覆盖函数定义 + file iterator + iterator body 内 call，`p12_stdin_echo` 使用 case 根目录 `stdin.txt` fixture，覆盖 `@stdin` line-iterator + `@stdout` write shorthand（`line >> @stdout`），`p13_stdin_transform` 也使用 case 根目录 `stdin.txt` fixture，覆盖 `@stdin` line-iterator + block 内算术 bind + `@stdout` write shorthand，`p14_stdin_pull` 使用 case 根目录 `stdin.txt` fixture，覆盖表达式级 `(<< @stdin)` + print，并冻结当前 `styio_cstr_to_i64()` 的标量 lowering 契约，`p15_stdin_mixed_output` 使用 case 根目录 `stdin.txt` fixture 与 `expected/stderr.txt`，覆盖 `@stdin` line-iterator + stdout/stderr 双通道 write shorthand；legacy/nightly 在 M1 算术、typed bind、compound assign、M2 simple func、M3 `t02_match_expr`、M5 `t01_read_file` / `t02_write_file` / `t05_auto_detect` / `t07_redirect` / `t08_pipe_func`、M7 `t01_zip_collections` / `t02_zip_unequal` / `t03_snapshot` / `t04_instant_pull` / `t05_zip_files` / `t08_arbitrage` / `t10_full_pipeline` 与未标注参数函数样例上一致；hash 无箭头表达式体/无赋值箭头体/`?=` match-cases 样例上一致；`# ... >> ...` hash iterator 定义样例在双引擎下可执行且无崩溃（退出码 `0`，输出一致）；`# ... >> ... ?= ... => ...` 组合链路稳定返回 `ParseError`（退出码 `3`）且不再出现 `Styio.NotImplemented` 前缀；`?= {}` 空 cases 当前语义稳定拒绝并返回 `ParseError`（exit `3`）；`match` 非整型 scrutinee（流式 `line ?={...}`）稳定返回 `TypeError`（退出码 `4`）且不再 abort；无默认分支 `?=`（`x = 1; x ?= { 1 => >_(1) }`）不再崩溃并保持稳定输出；malformed 语句前缀（fuzz 样本）稳定返回 `ParseError`（不再 signal 退出）；`x : i64 := 1` 后 `x += 2` 路径稳定返回 `TypeError`（退出码 `4`，不再误报 `RuntimeError`）；`a = [1,2,3]` 参与 zip 且来源组合超出支持边界时稳定返回 `TypeError`（退出码 `4`，不再误报 `RuntimeError`）；`@[3](ma = p[avg, n])` 非字面量窗口参数稳定返回 `TypeError` 且不再出现 `Styio.NotImplemented`；`# pulse = (x) => @[sum = 0](out = $sum + x)` 在 pulse 体中调用时稳定输出 `1/3/6`（参数替换生效）；`# pulse = (x) => @[sum = 0](out = x ?= {...})` 在 pulse 体中调用时稳定输出 `10/12/15`（`MatchCases/Cases` 克隆可用）；`# pulse = (x) => @[sum = 0](out = [...])` 在 pulse 体中调用时稳定输出 `0/0`（`InfiniteAST` 克隆可用）；默认 parser 引擎为 `nightly`（`DefaultEngineIsNightlyInShadowArtifact` 冻结契约），同时保留 `--parser-engine=new` 作为兼容别名，且可通过 `--parser-engine=legacy` 回退；非法引擎拒绝；`--parser-shadow-compare` 可通过 M1 核心/全量、M2 core/full、M5 资源/iterator 样例，以及 M7 zip-collections / zip-unequal / snapshot / instant-pull / at-resource 样例；混合程序下 shadow artifact `detail` 会记录 `primary_route=nightly_subset_statements=...,legacy_fallback_statements=...,nightly_internal_legacy_bridges=...`，并分别通过 mixed-route、match-cases、资源 postfix、name-led iterator、`@[...]` snapshot decl、`@file... >> ...` 资源流、`[ ... ] >> ...` list-start iterator 与“绑定后换行起 list”样例冻结零回退或零 internal bridge；`--parser-shadow-artifact-dir` 可产出 JSONL 记录并受参数约束回归覆盖；dot-chain 边界在 `DotChainStillRejectedConsistentlyAcrossEngines` 冻结；runtime helper 失败在 `--error-format=jsonl` 下稳定输出 `RuntimeError/STYIO_RUNTIME`，并覆盖 `STYIO_RUNTIME_FILE_OPEN_READ|STYIO_RUNTIME_FILE_OPEN_WRITE` 子码；CI 通过 `scripts/parser-shadow-suite-gate.sh` 对 M1/M2/M5/M7 目录执行 gate 并产出 `summary.json`） | `ctest --test-dir build -L styio_pipeline` 或 `ctest --test-dir build -R '^Styio(ParserEngine|Diagnostics)\\.'` |

**五层流水线 goldens**（Lexer / AST / StyioIR / LLVM / 子进程 stdout）：权威说明见 [`FIVE-LAYER-PIPELINE.md`](./FIVE-LAYER-PIPELINE.md)；用例根目录 `tests/pipeline_cases/`。

**M1/M2 dual-zero gates：** `ctest --test-dir build -R '^parser_shadow_gate_m(1|2)_zero_fallback_and_internal_bridges$'`。两条测试都执行 `scripts/parser-shadow-suite-gate.sh --require-zero-fallback --require-zero-internal-bridges`，要求对应目录全套 shadow artifact 为 `match`，且同时满足 `legacy_fallback_statements=0` 与 `nightly_internal_legacy_bridges=0`。

**M5 dual-zero gate with expected nonzero manifest：** `ctest --test-dir build -R '^parser_shadow_gate_m5_dual_zero_expected_nonzero$'`。该测试执行 `scripts/parser-shadow-suite-gate.sh --require-zero-fallback --require-zero-internal-bridges`，并读取 [`tests/milestones/m5/shadow-expected-nonzero.txt`](../../../tests/milestones/m5/shadow-expected-nonzero.txt)。manifest 中样例允许非零退出，但仍必须产出 `status=match` 的 shadow artifact；当前登记 `t06_fail_fast`。

**M7 zero-fallback gate：** `ctest --test-dir build -R '^parser_shadow_gate_m7_zero_fallback$'`。该测试执行 `scripts/parser-shadow-suite-gate.sh --require-zero-fallback`，要求 `tests/milestones/m7` 全套 shadow artifact 为 `match`，且 `legacy_fallback_statements=0`。

**M7 zero-internal-bridges gate：** `ctest --test-dir build -R '^parser_shadow_gate_m7_zero_internal_bridges$'`。该测试执行 `scripts/parser-shadow-suite-gate.sh --require-zero-fallback --require-zero-internal-bridges`，要求 `tests/milestones/m7` 全套 shadow artifact 为 `match`，且 `nightly_internal_legacy_bridges=0`。

**Parser legacy entry audit：** `ctest --test-dir build -R '^parser_legacy_entry_audit$'`。该测试执行 [`scripts/parser-legacy-entry-audit.sh`](../../../scripts/parser-legacy-entry-audit.sh)，要求 `src/` 中除 parser core 外不再直接调用 `parse_main_block_legacy(...)`，并冻结 `src/StyioTesting/` 为 nightly-first，防止验证/生产路径回连 legacy。

与里程碑 `.styio` 仅比对最终输出的集成测试 **互补**；历史实现细节另见 [`../../specs/AGENT-SPEC.md`](../../specs/AGENT-SPEC.md) §10。

---

## 10. Fuzz 测试（libFuzzer）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_fuzz_lexer` | 词法器随机输入健壮性，输入入口 `tests/fuzz/corpus/lexer` | `ctest --test-dir build-fuzz -R '^fuzz_lexer_smoke$'` |
| `styio_fuzz_parser` | parser 随机输入健壮性，输入入口 `tests/fuzz/corpus/parser`；同一输入顺序驱动 `legacy` 与 `nightly` 两条 parser 路由 | `ctest --test-dir build-fuzz -R '^fuzz_parser_smoke$'` |
| `fuzz_regression_pack_smoke` | `scripts/fuzz-regression-pack.sh` 回流打包链路自检（manifest/summary/corpus-backflow 生成） | `ctest --test-dir build-fuzz -R '^fuzz_regression_pack_smoke$'` |

**构建开关：** `-DSTYIO_ENABLE_FUZZ=ON`（默认 OFF）。
**PR 短跑：** `ctest --test-dir build-fuzz -L fuzz_smoke`。
**夜间深跑：** 见 `.github/workflows/nightly-fuzz.yml`（`fuzz-artifacts/` + `fuzz-regressions/` 双工件归档）。
**环境注意：** 在 macOS 上本地跑 fuzz 建议使用 `clang-18` + `CMAKE_OSX_SYSROOT`；`fuzz_smoke` 已设置 `ASAN_OPTIONS=detect_container_overflow=0` 以规避 libFuzzer 运行时 container-overflow 误报噪音。
**失败样本打包：** `./scripts/fuzz-regression-pack.sh --artifacts-root ./fuzz-artifacts --out-dir ./fuzz-regressions --run-id <id>`。
**仓库保护：** `fuzz_lexer_smoke` / `fuzz_parser_smoke` 运行时复制临时 corpus，不直接写入 `tests/fuzz/corpus/`。

---

## 11. Soak 测试（单线程压力框架）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_soak_test` | `tests/soak/styio_soak_test.cpp`：长输入词法循环、文件句柄生命周期循环、文件/拼接 RSS 增长阈值守卫、非法句柄高频诊断循环、M6 流式程序重复执行、单参数 state helper（直返 `StateDecl`）内联链路长跑回归（输出 `1/3/6`）、state helper `MatchCases/Cases` 内联链路长跑回归（输出 `10/12/15`）、state helper `InfiniteAST` 内联链路长跑回归（输出 `0/0`） | `ctest --test-dir build -L soak_smoke` |

**构建：** `cmake --build build --target styio_soak_test`。  
**默认档位：** smoke（PR/CI 执行 `ctest --test-dir build -L soak_smoke`）。  
**夜间深跑：** `ctest --test-dir build -L soak_deep`（工作流见 `.github/workflows/nightly-soak.yml`，日志归档）。  
**放大量参数：** 由 `soak_deep` 用例注入 `STYIO_SOAK_*` 环境变量，详见 [`tests/soak/README.md`](../../../tests/soak/README.md)。  
**恢复一键检查：** `./scripts/checkpoint-health.sh --no-asan`（含 state-inline soak deep + `styio_pipeline` + `security` + `parser_shadow_gate_m1_zero_fallback_and_internal_bridges` + `parser_shadow_gate_m2_zero_fallback_and_internal_bridges` + `parser_shadow_gate_m5_dual_zero_expected_nonzero` + `parser_shadow_gate_m7_zero_fallback` + `parser_shadow_gate_m7_zero_internal_bridges`；若检测到 `build-fuzz`，还会自动跑 `fuzz_smoke`）。默认优先尝试 `build/`，若该目录 cache 失效会自动回退到 `build-codex/`。需要显式指定时可用 `--fuzz-build-dir build-fuzz`；不想跑 fuzz 可加 `--no-fuzz`。
**失败最小化：** `./scripts/soak-minimize.sh ...` 生成 `tests/soak/regressions/<timestamp>-<case>/`，并按 [`tests/soak/REGRESSION-TEMPLATE.md`](../../../tests/soak/REGRESSION-TEMPLATE.md) 记录回归。

---

## 12. Security/Safety 回归集（标签）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_security_test` | `tests/security/styio_security_test.cpp`：lexer/Unicode/AST ownership/runtime（含 `StyioSafetyRuntime.*` 内存与 FFI 生命周期回归、`StyioSafetyHandleTable.*` 句柄表语义回归；覆盖“非法非零句柄可诊断、close 幂等、kind mismatch/stub 清理、runtime last-error 清理契约、path-null/open-read/open-write 子码稳定、first-error-wins 子码不漂移”），含 `StyioSecurityParserContext.*`（空 token 向量 EOF 降级、`move_forward` 越界钳制、`peak_operator` EOF 安全返回）与 `StyioSecurityParserPath.*`（单字符路径不触发 `out_of_range`）边界回归、`StyioSecurityAstOwnership` 中 `PrintAST`/`StateRefAST`/`HistoryProbeAST`/`SeriesIntrinsicAST`/`StateDeclAST`/`CasesAST`/`MatchCasesAST`/`CondFlowAST`/`FunctionAST`/`SimpleFuncAST`/`InfiniteLoopAST`/`StreamZipAST`/`IteratorAST`/`BlockAST`/`MainBlockAST`/`CheckIsinAST`/`InfiniteAST`/`AnonyFuncAST`/`SnapshotDeclAST`/`InstantPullAST`/`IterSeqAST`/`ExtractorAST`/`BackwardAST`/`CODPAST`/`ForwardAST` 析构所有权回归、ParserLookahead trivia 回归、nightly parser expr 子集兼容回归（实现位于 `NewParserExpr.*`，含 compare/logic/dot-call token gate、表达式 postfix `?={...}`、资源 postfix `>> @file{...}` / `-> @file{...}`、instant pull `(<< @file{...})`、`[ ... ]` list-start / infinite collection atom 与换行边界）、nightly parser stmt（print/flex bind/final bind/compound assign/compare/logic/simple call/dot-call/function-def-entry/hash-simple-func，含 `[|n|]`、tuple 返回类型、`=> >_(...)` 语句体、`= expr` 无箭头函数体、无赋值 `=>` 函数体、`?=` match-cases、standalone block/control 子集 `{}`/`<<`/`^`/`>>`/`...`/`|>`、`@[...]` snapshot/state-decl 起始、`@file{...}` / `@{...}` 资源流起始、name-led iterator 语句、list-start iterator 语句、`InfiniteAST` loop body 语句，以及 `# ... >> ...` iterator 定义样例）子集回归与 Shadow fallback 回归（dot-chain） | `ctest --test-dir build -L security` 或 `ctest --test-dir build -L safety` |

---

## 13. Sanitizer 夜间门禁

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio-nightly-sanitizers` | `.github/workflows/nightly-sanitizers.yml`：`clang-18` + `ASan/UBSan` 构建并执行 `security`、`soak_smoke`、`styio_pipeline`、`milestone` | GitHub Actions `workflow_dispatch` 或 nightly cron |
