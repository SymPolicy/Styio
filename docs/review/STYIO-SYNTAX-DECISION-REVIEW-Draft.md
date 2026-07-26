# Styio 语言设计问题集合

**Purpose:** 只维护仍需语言所有者决定的高层能力包。已批准语义进入
`docs/design/` 的唯一所有者文档与决策账本。

**Last updated:** 2026-07-26

**Status:** Draft — four future capability packages remain; foundation
packages are closed.

## 1. 讨论规则

1. 一次只讨论一个高层能力包，并且只问一个所有者问题。
2. 运算符表、类型关系、具体方法名和单项语法不是独立所有者问题。
3. 已批准结论移入权威设计文档，本文件仅保留状态与链接。
4. 未被高层能力包准入的语法默认不存在。
5. 当前 parser、Sema、IR 或 runtime 行为不能替代语言决策。

## 2. 已关闭基础

以下基础不再重新提问：

- 缺值、Unit、bottom、Block 完成、方向流与有限名义 completion；
- 严格值求值、依赖图顺序、value/affine-owner/borrow 与提交边界；
- 精确数值字面量、完整 signed/unsigned 固定整数族、严格浮点、
  异构数值推断与 `expr :> T`；
- Unicode scalar、grapheme `char`、UTF-8 `string`、XID+NFC 标识符，以及
  `bytes`/`bits`/`blob`；
- 类型名是普通类型命名空间标识符，不是 lexer keyword。

权威索引：
[Styio Language Decision Ledger](../design/Styio-Language-Decision-Ledger.md)。

## 3. 已关闭高层能力包

| ID | 结果摘要 | 权威 |
|---|---|---|
| `D1-DATA` | tuple 结构型；record/variant 名义型；pattern 保持身份与所有权；closed match 穷尽；FFI/layout 显式适配 | [Data and Collection Model](../design/Styio-Data-and-Collection-Model.md) |
| `D2-COLLECTIONS` | materialized collection 递归值语义；普通 slice 为稳定快照；view 显式借用；iterator/stream 与 collection 分离；顺序确定；`list[T] != T..` | [Data and Collection Model](../design/Styio-Data-and-Collection-Model.md) |
| `D3-RESOURCES` | capability、typestate、owner 和 exit obligation 正交；结构化 task；v1 无 detach；有限背压；EOF/absence/failure/cancel/pressure 分离 | [Structured Resources and Concurrency](../design/Styio-Structured-Resources-and-Concurrency.md) |
| `D4-MODULES` | 规范包/模块身份；默认私有；显式 import/export/re-export；无 glob、dot-path 兼容和循环初始化；全局 coherence；最小 prelude | [Module and Extension Model](../design/Styio-Module-and-Extension-Model.md) |

codec、decode、JSON、CSV、数据库映射、Unicode normalization 和具体资源
driver 均属于显式导入的标准库，不进入 prelude。

## 4. 当前问题地图

剩余讨论只按以下顺序进行：

| 顺序 | ID | 高层能力包 | 唯一核心问题 |
|---:|---|---|---|
| 1 | `F1-ABSTRACTION` | 推断与扩展 | 如何在最大化后台推断的同时保证用户扩展唯一、安全且可预测 |
| 2 | `F2-META` | 编译期能力 | 哪些程序可以在编译期执行、检查或生成代码 |
| 3 | `F3-SYSTEMS` | 原生边界 | FFI、ABI、布局、指针和 unsafe 如何隔离在安全核心之外 |
| 4 | `F4-ADVANCED` | 高级运行时能力 | continuation、effect handler、detached task、动态加载和反射是否值得准入 |

未进入这四包且没有具体用例的实验特性默认不准入。

## 5. F1-ABSTRACTION — 推断与扩展

已经批准：

- 不提供 `[T]`、`[Item: type]`、`forall` 等作者泛型参数表；
- 可唯一求解的 rank-1 泛型关系在定义点后台推断；
- 稳定 public 主类型可以写入规范模块接口；
- 能力需求从函数体推断，用户类型的具体协议实现必须显式且 coherent；
- 不由第一次调用、导入顺序或后端状态决定类型；
- 单态化和实例预算确定，运行时无泛型字典。

仍由本包统一决定，不拆成单项提问：

- generic data type 的定义与递归边界；
- 用户类型参与既有运算符；
- 用户定义转换；
- associated type、方法解析和动态分派；
- 哪些扩展必须继续保持封闭。

权威：
[Inferred Abstraction and Explicit Conformance](../design/Styio-Inferred-Abstraction-and-Explicit-Conformance.md)。

**唯一所有者问题：Styio 的推断式抽象与显式扩展应采用怎样的统一模型，才能不要求用户重复类型关系，同时保持全局 coherence？**

## 6. F2-META — 编译期能力

本包统一决定 const evaluation、derive、macro、卫生、生成代码的身份、
增量缓存、资源预算、可重现性和编译期权限。不会分别询问每种宏括号或模板标记。

**唯一所有者问题：Styio 应允许哪些可确定、可缓存且无环境泄漏的编译期计算与代码生成？**

## 7. F3-SYSTEMS — 原生边界

本包统一决定 FFI、ABI、布局适配、raw pointer、unsafe region、SIMD、
平台类型和外部 owner/borrow 适配。安全 Styio 类型不会因布局相似而自动成为
原生兼容类型。

**唯一所有者问题：Styio 如何把不可证明的原生行为限制在显式、最小且可审计的边界内？**

## 8. F4-ADVANCED — 高级运行时能力

本包只在出现明确用例后统一评估 continuation、effect handler、脱离结构化
作用域的任务、运行时反射、动态模块加载和 monkey patching。批准其中一种不自动
准入其他能力。

**唯一所有者问题：哪些高级控制或动态能力提供了结构化模型无法表达的必要价值，并能保持确定的生命周期与失败边界？**

## 9. 关闭条件

一个能力包只有在以下条件同时满足时关闭：

1. 所有者批准一套统一原则；
2. `docs/design/` 存在唯一语义所有者；
3. EBNF、symbol reference 和 active syntax 只镜像该所有者；
4. 冲突旧路线有明确删除边界；
5. 有限正例、负例和一致性验证可以证明该原则。
