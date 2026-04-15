# `[|n|]` 环缓 CodeGen：从 i64 Bootstrap 迁移的调整方案

**Purpose:** 记录在 **`bounded_ring:n`** 类型上 **从「整段降级为单 i64」改为「栈上 `[n x i64]` + 写指针」** 时的 **破坏面、语义约定、测试与回滚步骤**；冷启动摘要见 [`../rollups/HISTORICAL-LESSONS.md`](../rollups/HISTORICAL-LESSONS.md)，精确会话原文见 [`../archive/history/2026-03-29.md`](../archive/history/2026-03-29.md)。

**Last updated:** 2026-03-28

---

## 1. 为什么允许暂时破坏 Bootstrap

- **旧行为：** `TypeAST::CreateBoundedRingBuffer` 使用 `Defined` + 名 `bounded_ring:n`，`GetTypeG` 对未知 `Defined` **回退 i64**，`SGFinalBind` 与标量绑定无异 —— **便于快速打通词法/解析**，但 **不承载** Topology 文档中的有界环语义。
- **新行为：** 同一 `StyioDataType` 拼写不变，**LLVM 层**为 `alloca [n x i64]` + `alloca i64`（head，表示下一写入槽下标），**读取** `x` 时返回 **最近一次写入的槽**（head 为 0 时读作 0，避免未初始化歧义）。

---

## 2. 破坏面（Breaking / 风险）

| 区域 | 影响 |
|------|------|
| **内存布局** | 同名 `x` 在 LLVM 中由「单 i64 alloca」变为「数组 + head」；**IR dump、sizeof 类假设** 若存在会不一致。 |
| **`named_values`** | 环绑 **不再** 把初值 SSA 塞进 `named_values`（避免绕过环槽）；仅依赖 `mutable_variables` + `bounded_ring_head_slot_`。 |
| **`x = …`（FlexBind）** | 凡 **`x` 曾用 `x : T := …`（final / fixed）绑定**，再在顶层或 `>>` / `&` 体内对 **`x` 做 flex（`=`）**，**类型检查阶段报错**（`TypeInfer.cpp`）；**仍禁止**依赖「另一套标量 alloca」偷偷更新环槽。 |
| **`#(a : [|n|])` 形参** | `toLLVMType(SGVar)` 对环返回 **逻辑 i64**，形参槽为标量；与 **final bind 环存储** 不一致 —— **函数参数上的 `[|n|]` 仍视为占位/未完整**，仅保证 **顶层 final bind + 读取** 用例。 |
| **极大 `n`** | `styio_bounded_ring_capacity` 拒绝 0；超大 `n` 可能导致栈过大 —— 后续应在语义层加 **上限**（与 ledger 设计对齐）。 |

---

## 3. 语义约定（本阶段）

1. **初始化 `x : [|n|] := v`：** 写入 `slot[0] = v`，`head = 1`（下一写入为 `1 mod n`）。  
2. **读取 `x`：** 若 `head > 0`，下标为 `(head - 1) mod n`；否则值为 **0**。  
3. **多次写入：** 尚未暴露 `->` / 赋值到环的语句面；后续应用 `head` 做环形推进并与 **Topology §5** 对齐。

---

## 4. 测试调整策略

| 动作 | 说明 |
|------|------|
| **保留** `m8_t01_bounded_final_bind` | 仍只打印字面量 `1`，与环语义无冲突。 |
| **新增** `m8_t02_bounded_read` | `>_(x)` 期望 **42**，验证 **读路径**。 |
| **全量 milestone** | 合并前跑 `ctest -L milestone`；若 m5/m7 等因环境失败，与本次改动 **无直接关系** 时需单独记录。 |
| **golden 更新** | 仅当 **stdout 语义** 变化时重生成 `expected/*.out`；禁止无原因改 golden。 |

---

## 5. 回滚方案

1. **Git 回退** 涉及 `CodeGenG.cpp`、`GetTypeG.cpp`、`CodeGenVisitor.hpp`、`StyioUtil/BoundedType.hpp` 的提交。  
2. **恢复** `toLLVMType(SGType*)` 对 `bounded_ring:` **不** 返回 `ArrayType`（改回 default i64）。  
3. **恢复** `SGFinalBind` 单 alloca + `named_values` 旧路径。  
4. **删除** `t02` 或将其期望改回与旧语义一致（不推荐，除非放弃环语义）。

---

## 6. 后续扩展（与计划文档对齐）

- 将 **head 推进** 挂到 **`expr -> $x`** 或专用 push 语句。  
- **顶层 `ResourceDecl`** 与 **匿名 ledger** 合并，替换栈上数组（多函数/线程模型）。  
- 在 **`../review/Logic-Conflicts.md`** 中关闭「`$x =` vs `->`」相关条目时引用本节语义。

---

## 7. SSOT 关系

- 设计目标：[`../design/Styio-Resource-Topology.md`](../design/Styio-Resource-Topology.md)  
- 总实施清单：[`Resource-Topology-v2-Implementation-Plan.md`](./Resource-Topology-v2-Implementation-Plan.md)  
- 类型名前缀解析：[`src/StyioUtil/BoundedType.hpp`](../../src/StyioUtil/BoundedType.hpp)
