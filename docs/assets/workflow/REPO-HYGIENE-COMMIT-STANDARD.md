# Styio 仓库清理与提交标准

**Purpose:** 约束 Styio 仓库的本地清理、`.gitignore`、提交前检查、push 前检查、历史重写与 `force-push` 边界；不定义语言语义与功能重构步骤（见 `CHECKPOINT-WORKFLOW.md` / `../templates/REFACTOR-WORKFLOW-TEMPLATE.md`）。

**Last updated:** 2026-04-08

---

## 1. 强制规则

1. 构建目录、测试发现产物、二进制、静态库、临时日志、profile、sanitizer/fuzz 残留一律不得进入版本库。
2. 任何生成文件都必须先由 `.gitignore` 覆盖，再允许开发者本地生成。
3. 任何非源码 blob 一旦接近 GitHub `100MB` 硬限制，就不允许进入当前分支历史；Styio 的项目内软阈值定为 `20MB`。
4. 若 push 被 GitHub 以大文件拒绝，不能只删工作区文件，必须把该文件从**当前待推送历史**里移除。
5. 共享分支禁止随意改写历史；个人工作分支允许 `force-with-lease`，但必须先完成大文件与垃圾产物检查。

---

## 2. 禁止提交的内容

典型禁止项：

1. 构建输出：
   - `build/`
   - `build-*`
   - `Testing/`
   - `CMakeFiles/`
   - `CMakeCache.txt`
2. 测试发现/临时产物：
   - `cmake_test_discovery_*.json`
   - `*.tmp`
   - `*.log`
3. 编译产物：
   - `*.o`
   - `*.obj`
   - `*.a`
   - `*.so`
   - `*.dylib`
   - `*.dll`
   - `*.exe`
   - `*.dSYM/`
4. 本地调试输出：
   - `output.ll`
   - `sample.ll`
   - 本地生成的 `styio` / `styio_test` / `styio_security_test`
5. 本地工具缓存：
   - `.kilo/`
   - `.DS_Store`
   - sanitizer / coverage / profiling 输出

原则：

1. “生成得到的文件”默认禁止提交，除非它本身就是项目规定的冻结 artifact。
2. “体积大但看起来有用”的产物也默认禁止提交；是否保留要由文档和测试策略明示，而不是靠个人判断。

---

## 3. `.gitignore` 规则

1. 新增任何会落盘的生成目录或临时文件前，先更新 `.gitignore`。
2. 若发现某类垃圾文件已被跟踪：
   - 先补 `.gitignore`
   - 再执行 `git rm --cached <path>` 或 `git rm -r --cached <dir>`
3. `.gitignore` 规则不得误伤仓库真实源码/样例目录；例如文件名 `sample` 和目录 `sample/` 必须区分。

---

## 4. 提交前检查

每次提交前至少执行：

```bash
git status --short
git diff --cached --name-only
```

必须满足：

1. 暂存区只包含本次 checkpoint 的目标改动。
2. 不包含 `build*`、`Testing/`、`cmake_test_discovery_*.json`、二进制与库文件。
3. 不把“功能改动”和“误提交的垃圾产物清理”混成无法审查的黑盒，除非本次提交本身就是清理修复。

可选快速筛查：

```bash
git diff --cached --name-only \
  | rg '(^build($|/)|^build-|^Testing/|cmake_test_discovery_|\.o$|\.obj$|\.a$|\.so$|\.dylib$|\.dll$|\.exe$|\.dSYM/$)'
```

若有输出，先清理，再提交。

---

## 5. Push 前检查

### 5.1 大文件检查

若当前分支已绑定上游，执行：

```bash
git rev-list --objects @{u}..HEAD \
  | git cat-file --batch-check='%(objecttype) %(objectsize) %(rest)' \
  | awk '$1=="blob" && $2 >= 20000000 { print }'
```

若没有上游，把 `@{u}..HEAD` 改成 `HEAD`。

判定规则：

1. `>= 20MB`：必须人工确认；若是生成产物，直接清理。
2. `>= 100MB`：禁止 push，必须先改写当前待推送历史。

### 5.2 最小验证

至少满足以下之一：

1. 本次改动对应的最小测试集通过；
2. 若是流程/基础设施/里程碑收口，跑：

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

---

## 6. 清理流程

### 6.1 本地垃圾文件清理

```bash
rm -rf build build-* Testing .kilo
find . -name .DS_Store -delete
```

### 6.2 已跟踪生成文件摘除

```bash
git rm -r --cached build build-* Testing
git rm --cached cmake_test_discovery_*.json output.ll sample.ll
```

然后提交 `.gitignore` 与清理结果。

### 6.3 当前分支历史里已经混入大文件

适用条件：

1. GitHub push 因 `100MB` 被拒绝；
2. 大文件存在于 `@{u}..HEAD`，即当前待推送历史；
3. 不准备重写共享主线历史。

推荐做法：

1. 从上游分支拉出一条干净临时分支；
2. 逐个重放本地提交；
3. 在每次重放后把禁止路径从 index 和工作区剔除；
4. 核对 `@{u}..HEAD` 已不再包含大 blob；
5. 用 `git push --force-with-lease` 更新个人分支。

不推荐：

1. 为了删当前分支的大文件而重写整个仓库所有历史；
2. 直接 `git push --force` 而不做 lease 检查。

---

## 7. `force-push` 规则

1. 只允许对个人工作分支或明确约定可改写的分支使用 `git push --force-with-lease`。
2. 不允许对共享主分支直接 `force-push`。
3. 改写历史前必须确认工作区干净。
4. 改写历史后必须再跑一次“大文件检查”。

标准命令：

```bash
git push --force-with-lease origin <branch>
```

---

## 8. 提交粒度与消息

1. 一个提交只做一件可解释的事。
2. 误提交垃圾文件的清理，优先单独成提交；若它是修复 push 阻塞的必要条件，可以与 `.gitignore` 修正一起提交。
3. 生成产物清理不应掩盖功能逻辑变更。

推荐前缀：

```text
checkpoint: <范围> <目的>
cleanup: remove tracked generated artifacts
docs: freeze repo hygiene and commit standard
fix: <行为修复>
```

---

## 9. 完成标准

一次提交/推送可以视为“仓库卫生合格”，必须同时满足：

1. `git status` 干净，或只剩本次明确要提交的改动。
2. 暂存区里没有禁止提交的生成产物。
3. `@{u}..HEAD` 不含可疑大 blob，尤其没有 `>= 100MB` 文件。
4. `.gitignore` 已覆盖本次引入的所有新生成产物。
5. 对应测试或恢复脚本已跑通。

---

## 10. 与其它文档的关系

1. 重构拆刀、ADR、恢复记录：见 [`CHECKPOINT-WORKFLOW.md`](./CHECKPOINT-WORKFLOW.md)
2. 通用执行模板：见 [`../templates/REFACTOR-WORKFLOW-TEMPLATE.md`](../templates/REFACTOR-WORKFLOW-TEMPLATE.md)
3. 文档职责与 SSOT：见 [`../../specs/DOCUMENTATION-POLICY.md`](../../specs/DOCUMENTATION-POLICY.md)
