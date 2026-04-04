# Styio 测试目录（按功能域）

**文档作用：** 将 **里程碑集成测试** 按功能域映射到 **输入 `.styio`、golden/副作用路径与 `ctest` 命令**；权威自动化入口见 `tests/CMakeLists.txt`。维护规则见 [`DOCUMENTATION-POLICY.md`](../DOCUMENTATION-POLICY.md)。

**Last updated:** 2026-04-04（新增 soak 分档 + D.5 最小化模板，并补 ParserEngine/ParserLookahead/NewParserExpr/NewParserStmt（含 compound assign）/ShadowFallback 回归测试）

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

**整组：** `ctest --test-dir build -L m2`（仓库中无 `t08`，与里程碑编号可能不连续。）

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

**说明：** `[|n|]` 在 **final bind** 下生成 **`[n x i64]` + head**；**已对 `x` 使用 `:=` 后再写 `x = …`** 在 **typeInfer** 报错；`#` 形参环语义仍不完整，见 [`docs/BoundedRing-Codegen-Adjustment.md`](../BoundedRing-Codegen-Adjustment.md)。

**整组：** `ctest --test-dir build -L m8`

---

## 9. C++ 单元测试（GoogleTest）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_test` | `tests/styio_test.cpp`：`StyioFiveLayerPipeline`、`StyioParserEngine`（legacy/new 在 M1 算术、typed bind、compound assign 与 M2 simple func 样例上一致，非法引擎拒绝；`--parser-shadow-compare` 可通过 M1 核心/全量与 M2 core/full 样例；`--parser-shadow-artifact-dir` 可产出 JSONL 记录并受参数约束回归覆盖；dot-chain 边界在 `DotChainStillRejectedConsistentlyAcrossEngines` 冻结；CI 通过 `scripts/parser-shadow-m1-gate.sh`（M1）+ 显式 `ShadowCompareAcceptsM2FullSuite`（M2）双门槛阻断） | `ctest --test-dir build -L styio_pipeline` 或 `ctest --test-dir build -R '^StyioParserEngine\\.'` |

**五层流水线 goldens**（Lexer / AST / StyioIR / LLVM / 子进程 stdout）：权威说明见 [`FIVE-LAYER-PIPELINE.md`](./FIVE-LAYER-PIPELINE.md)；用例根目录 `tests/pipeline_cases/`。

与里程碑 `.styio` 仅比对最终输出的集成测试 **互补**；历史实现细节另见 `docs/AGENT-SPEC.md` §10。

---

## 10. Fuzz 测试（libFuzzer）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_fuzz_lexer` | 词法器随机输入健壮性，输入入口 `tests/fuzz/corpus/lexer` | `ctest --test-dir build -R '^fuzz_lexer_smoke$'` |
| `styio_fuzz_parser` | parser 随机输入健壮性，输入入口 `tests/fuzz/corpus/parser` | `ctest --test-dir build -R '^fuzz_parser_smoke$'` |

**构建开关：** `-DSTYIO_ENABLE_FUZZ=ON`（默认 OFF）。
**PR 短跑：** `ctest --test-dir build -L fuzz_smoke`。
**夜间深跑：** 见 `.github/workflows/nightly-fuzz.yml`（产物自动归档）。

---

## 11. Soak 测试（单线程压力框架）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_soak_test` | `tests/soak/styio_soak_test.cpp`：长输入词法循环、文件句柄生命周期循环、RSS 增长阈值守卫、M6 流式程序重复执行 | `ctest --test-dir build -L soak_smoke` |

**构建：** `cmake --build build --target styio_soak_test`。  
**默认档位：** smoke（PR/CI 执行 `ctest --test-dir build -L soak_smoke`）。  
**夜间深跑：** `ctest --test-dir build -L soak_deep`（工作流见 `.github/workflows/nightly-soak.yml`，日志归档）。  
**放大量参数：** 由 `soak_deep` 用例注入 `STYIO_SOAK_*` 环境变量，详见 [`tests/soak/README.md`](../../tests/soak/README.md)。
**失败最小化：** `./scripts/soak-minimize.sh ...` 生成 `tests/soak/regressions/<timestamp>-<case>/`，并按 [`tests/soak/REGRESSION-TEMPLATE.md`](../../tests/soak/REGRESSION-TEMPLATE.md) 记录回归。

---

## 12. Security 回归集（标签）

| 目标 | 说明 | Automation |
|------|------|------------|
| `styio_security_test` | `tests/security/styio_security_test.cpp`：lexer/Unicode/AST ownership/runtime，含 ParserLookahead trivia 回归、NewParserExpr 子集兼容回归（含 compare/logic/dot-call token gate）、NewParserStmt（print/flex bind/final bind/compound assign/compare/logic/simple call/dot-call/function-def-entry/hash-simple-func）子集回归与 Shadow fallback 回归（dot-chain） | `ctest --test-dir build -L security` |
