#!/usr/bin/env bash
# 在仓库根目录生成「Git 变更摘要 + CTest 结果」Markdown 报告。
# 用法：
#   ./scripts/dev-report.sh              # 尝试编译后跑 milestone + styio_pipeline
#   ./scripts/dev-report.sh --no-build   # 不编译，仅基于当前 build/
#   ./scripts/dev-report.sh --note "理由"  # 报告顶部附加说明
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

DO_BUILD=1
NOTE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      DO_BUILD=0
      shift
      ;;
    --note)
      if [[ $# -lt 2 ]]; then
        echo "$0: --note requires a value" >&2
        exit 1
      fi
      NOTE="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [--no-build] [--note \"text\"]" >&2
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

REPORT_DIR="$ROOT/reports"
mkdir -p "$REPORT_DIR"
TS_LOCAL="$(date +"%Y-%m-%d %H:%M:%S %z")"
TS_FILE="$(date -u +"%Y-%m-%dT%H-%M-%SZ")"
OUT="$REPORT_DIR/dev-report-${TS_FILE}.md"
LATEST="$REPORT_DIR/LATEST.md"

BUILD_DIR="$ROOT/build"

{
  echo "# Styio 修改与测试报告"
  echo
  echo "- **本地时间:** ${TS_LOCAL}"
  echo "- **仓库:** \`${ROOT}\`"
  if [[ -n "$NOTE" ]]; then
    echo "- **备注:** ${NOTE}"
  fi
  echo

  echo "## 1. Git 状态"
  echo
  if git -C "$ROOT" rev-parse --is-inside-work-tree &>/dev/null; then
    echo '```'
    git -C "$ROOT" status -sb || true
    echo '```'
    echo
    echo "### 最近提交"
    echo '```'
    git -C "$ROOT" log -1 --oneline 2>/dev/null || echo "(no commits)"
    echo '```'
    echo
    echo "### 工作区变更 (diff --stat)"
    echo '```'
    git -C "$ROOT" diff --stat || true
    echo '```'
    echo
    STAGED="$(git -C "$ROOT" diff --cached --stat 2>/dev/null || true)"
    if [[ -n "${STAGED// }" ]]; then
      echo "### 暂存区 (cached diff --stat)"
      echo '```'
      echo "$STAGED"
      echo '```'
      echo
    fi
  else
    echo "(非 Git 仓库，跳过)"
    echo
  fi

  echo "## 2. 构建"
  echo
  if [[ "$DO_BUILD" -eq 1 ]]; then
    if [[ ! -d "$BUILD_DIR" ]]; then
      echo "运行: \`cmake -S . -B build\`（创建 build 目录）"
      cmake -S "$ROOT" -B "$BUILD_DIR" 2>&1 || true
    fi
    echo "运行: \`cmake --build build -j\$(nproc)\`（目标: styio styio_test）"
    if cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)" --target styio styio_test 2>&1; then
      echo "**结果:** 成功"
    else
      echo "**结果:** 失败（见上方日志）"
    fi
  else
    echo "已跳过构建 (\`--no-build\`)。"
  fi
  echo

  echo "## 3. 测试 (CTest)"
  echo
  if [[ ! -f "$BUILD_DIR/CTestTestfile.cmake" ]]; then
    echo "未找到 \`build/CTestTestfile.cmake\`。请先 \`cmake -S . -B build\` 并配置测试。"
  else
    echo "### 标签 \`milestone\`"
    echo '```'
    if ctest --test-dir "$BUILD_DIR" -L milestone --output-on-failure 2>&1; then
      echo '```'
      echo "**milestone:** 通过"
    else
      echo '```'
      echo "**milestone:** 有失败（见上方输出）"
    fi
    echo
    echo "### 标签 \`styio_pipeline\`（五层流水线）"
    echo '```'
    if ctest --test-dir "$BUILD_DIR" -L styio_pipeline --output-on-failure 2>&1; then
      echo '```'
      echo "**styio_pipeline:** 通过"
    else
      echo '```'
      echo "**styio_pipeline:** 有失败或未注册（见上方输出）"
    fi
  fi
  echo
  echo "---"
  echo
  echo "*由 \`scripts/dev-report.sh\` 生成。亦可复制本文件到 \`docs/history/\` 当日记录。*"
} | tee "$OUT"

cp -f "$OUT" "$LATEST"
echo >&2 "Report: $OUT"
echo >&2 "Latest: $LATEST"
