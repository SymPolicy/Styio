# 开发历史索引

**文档作用：** **索引** `docs/history/*.md` 按日开发记录；不写具体技术细节（细节在对应日期文件中）。策略见 [`DOCUMENTATION-POLICY.md`](../DOCUMENTATION-POLICY.md) §0。

**Last updated:** 2026-04-07

按日期记录的实现经验与进展见本目录下 `YYYY-MM-DD.md`。新内容请 **按当天日期新建或追加**，并在文首维护 **Last updated**。

| 文件 | 摘要 |
|------|------|
| [`2026-04-07.md`](2026-04-07.md) | C.5 收口：state helper 内联克隆补齐 `MatchCases/Cases` 与 `InfiniteAST`，修复 `unsupported AST node in inlined state expression clone`，新增 `StateInlineMatchCasesFunctionUsesCallArgument` / `StateInlineInfiniteLiteralFunctionUsesCallArgument` 回归并在 `styio_pipeline/security` 与 ASan/UBSan 下验证；D.3 新增 `StateInlineMatchCasesProgramLoop`、`StateInlineInfiniteProgramLoop` 与对应 `soak_deep` 长跑门禁；E.6 将 parser 默认引擎切换到 `new` 并新增 `DefaultEngineIsNewInShadowArtifact` 契约回归；D.5 新增一键恢复脚本 `scripts/checkpoint-health.sh`；E.7 继续压缩 parser fallback：`new` 子集原生接管 block/control 语句、表达式 postfix `?={...}`、资源 postfix `>> @file{...}` / `-> @file{...}`、name-led iterator / instant pull、`@[...]` snapshot/state-decl 起始，以及 `@file... >> ...` 资源流入口，并将主入口从整文件回退改为逐语句回退，同时把 route stats 写入 shadow artifact |
| [`2026-04-06.md`](2026-04-06.md) | F.2 收口：`SGStreamZip` 误分类修复并完成活跃流水线 `Styio.NotImplemented` 清零；F.3 修复 `?=` 无默认分支崩溃、ParserContext EOF 降级与越界前移钳制，并继续收口 `parse_path`/`peak_operator` 的 `out_of_range` 异常；C.3-C.5 持续推进 `CasesAST`/`MatchCasesAST`/`CondFlowAST`/`FunctionAST`/`SimpleFuncAST`/`InfiniteLoopAST`/`StreamZipAST`/`IteratorAST`/`BlockAST`/`MainBlockAST`/`CheckIsinAST`/`InfiniteAST`/`AnonyFuncAST`/`SnapshotDeclAST`/`InstantPullAST`/`IterSeqAST`/`ExtractorAST`/`BackwardAST`/`CODPAST`/`ForwardAST`/`PrintAST`/`StateRefAST`/`HistoryProbeAST`/`SeriesIntrinsicAST`/`StateDeclAST`/`TypeTupleAST`/`CheckEqualAST` RAII 所有权，并在 state inlining 路径引入克隆替换边界；C.5 新增单参数 state helper（直返 `StateDecl`）内联识别与参数替换回归，D.3 新增 `StateInlineHelperProgramLoop` / `soak_deep_state_inline_program` 长跑门禁，C.6 在 ASan/UBSan 下保持 `styio_pipeline/security` 全绿；B.4 联动修复 parser fuzz 崩溃并打通 fuzz smoke；B.5/D.5 新增 nightly fuzz case pack 回流闭环 |
| [`2026-04-05.md`](2026-04-05.md) | E.7 风险收口与恢复：先 guard 收敛崩溃，再定位 `ParamAST` 空类型根因并恢复 hash `>>` 定义；补 `match` 非整型 scrutinee 崩溃防线 |
| [`2026-04-04.md`](2026-04-04.md) | D.1-D.5 Soak 闭环 + E.1/E.2 parser 双轨基础（引擎路由与共享前瞻工具） |
| [`2026-04-03.md`](2026-04-03.md) | Checkpoint 执行：构建基线、错误模型、会话所有权空壳、CI 与 ADR 落地 |
| [`2026-03-29.md`](2026-03-29.md) | M1–M7 实现要点、调试经验与文档交叉引用 |
| [`2026-03-28.md`](2026-03-28.md) | 文档策略：history / milestones / 测试目录与 CTest 对齐 |

策略说明：[`../DOCUMENTATION-POLICY.md`](../DOCUMENTATION-POLICY.md)
