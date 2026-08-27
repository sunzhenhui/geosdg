#!/usr/bin/env bash
# ============================================================
# sync-agent-config.sh
# 同步 agent/ 目录下的 Skills、Commands、Rules 到不同
# AI 工具的标准化配置目录中：CodeBuddy / Claude Code / OpenAI Codex
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
AGENT_DIR="$PROJECT_ROOT/agent"

# ---------- 颜色输出 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

log_info()  { echo -e "${GREEN}[INFO]${NC}  $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_title() { echo -e "\n${CYAN}━━━ $1 ━━━${NC}"; }

# ---------- 工具函数 ----------
# 递归复制目录，覆盖目标
sync_dir() {
    local src="$1"
    local dst="$2"
    local label="$3"

    if [[ ! -d "$src" ]]; then
        log_warn "跳过 ${label}: 源目录不存在 ($src)"
        return
    fi

    mkdir -p "$dst"
    # 使用 rsync 精确同步：删除目标中源不存在的文件，保持权限
    if command -v rsync &>/dev/null; then
        rsync -a --delete "$src/" "$dst/"
    else
        cp -R "$src/"* "$dst/" 2>/dev/null || true
    fi
    log_info "${label} 已同步"
}

# ---------- 主逻辑 ----------
main() {
    local targets=()

    # 解析参数
    if [[ $# -eq 0 ]]; then
        # 默认同步所有
        targets=("codebuddy" "workbuddy" "claude" "codex")
    else
        for arg in "$@"; do
            case "$arg" in
                codebuddy|workbuddy|claude|codex) targets+=("$arg") ;;
                all) targets=("codebuddy" "workbuddy" "claude" "codex") ;;
                *) log_error "未知目标: $arg (支持: codebuddy, workbuddy, claude, codex, all)" ; exit 1 ;;
            esac
        done
    fi

    echo ""
    echo "╔══════════════════════════════════════════════╗"
    echo "║     Agent Config 同步工具 v1.1               ║"
    echo "║   同步目标: ${targets[*]}"
    echo "╚══════════════════════════════════════════════╝"
    echo ""

    local has_error=false

    for target in "${targets[@]}"; do
        case "$target" in
            codebuddy) sync_codebuddy || has_error=true ;;
            workbuddy) sync_workbuddy || has_error=true ;;
            claude)    sync_claude    || has_error=true ;;
            codex)     sync_codex     || has_error=true ;;
        esac
    done

    echo ""
    if $has_error; then
        log_error "部分同步失败，请检查上方日志"
        exit 1
    else
        log_info "全部同步完成 ✓"
    fi
}

# ============================================================
# CodeBuddy (.codebuddy/)
#   项目级: .codebuddy/skills/        ← agent/skills/
#            .codebuddy/commands/     ← agent/commands/
#            .codebuddy/rules/        ← agent/rules/
#            .codebuddy/settings.json ← agent/codebuddy-settings.json
#   用户级: ~/.codebuddy/...  (不在本脚本管理范围)
# ============================================================
sync_codebuddy() {
    log_title "同步 CodeBuddy (.codebuddy/)"
    local base="$PROJECT_ROOT/.codebuddy"

    # Skills
    sync_dir "$AGENT_DIR/skills" "$base/skills" "  Skills   → .codebuddy/skills/"
    # Commands
    sync_dir "$AGENT_DIR/commands" "$base/commands" "  Commands → .codebuddy/commands/"
    # Rules
    sync_dir "$AGENT_DIR/rules"  "$base/rules"  "  Rules    → .codebuddy/rules/"
    # Settings
    if [[ -f "$AGENT_DIR/codebuddy-settings.json" ]]; then
        mkdir -p "$base"
        cp "$AGENT_DIR/codebuddy-settings.json" "$base/settings.json"
        log_info "  Settings → .codebuddy/settings.json"
    fi
}

# ============================================================
# WorkBuddy (.workbuddy/)
#   项目级: .workbuddy/skills/        ← agent/skills/
#            .workbuddy/commands/     ← agent/commands/
#            .workbuddy/rules/        ← agent/rules/
#            .workbuddy/settings.json ← agent/codebuddy-settings.json (复用)
#   用户级: ~/.workbuddy/...  (不在本脚本管理范围)
#   注意: WorkBuddy 与 CodeBuddy 同属 buddy 系，目录结构一致，
#         复用同一份 settings 配置。
# ============================================================
sync_workbuddy() {
    log_title "同步 WorkBuddy (.workbuddy/)"
    local base="$PROJECT_ROOT/.workbuddy"

    # Skills
    sync_dir "$AGENT_DIR/skills" "$base/skills" "  Skills   → .workbuddy/skills/"
    # Commands
    sync_dir "$AGENT_DIR/commands" "$base/commands" "  Commands → .workbuddy/commands/"
    # Rules
    sync_dir "$AGENT_DIR/rules"  "$base/rules"  "  Rules    → .workbuddy/rules/"
    # Settings (复用 codebuddy-settings.json)
    if [[ -f "$AGENT_DIR/codebuddy-settings.json" ]]; then
        mkdir -p "$base"
        cp "$AGENT_DIR/codebuddy-settings.json" "$base/settings.json"
        log_info "  Settings → .workbuddy/settings.json"
    fi
}

# ============================================================
# Claude Code (.claude/)
#   项目级: .claude/skills/    ← agent/skills/
#           .claude/commands/  ← agent/commands/
#           .claude/rules/     ← agent/rules/
#   用户级: ~/.claude/skills/  (不在本脚本管理范围)
#   注意: Skills 优先于 Commands（同名冲突时 Skills 优先）
# ============================================================
sync_claude() {
    log_title "同步 Claude Code (.claude/)"
    local base="$PROJECT_ROOT/.claude"

    # Skills
    sync_dir "$AGENT_DIR/skills"   "$base/skills"   "  Skills   → .claude/skills/"
    # Commands
    sync_dir "$AGENT_DIR/commands" "$base/commands" "  Commands → .claude/commands/"
    # Rules
    sync_dir "$AGENT_DIR/rules"    "$base/rules"    "  Rules    → .claude/rules/"
}

# ============================================================
# OpenAI Codex (.agents/)
#   项目级: .agents/skills/    ← agent/skills/  (Codex 使用 .agents 而非 .codex)
#   AGENTS.md   ← agent/rules/* 拼接 (项目根目录的长期规则/记忆文件)
#   用户级: ~/.agents/skills/  (不在本脚本管理范围)
#
#   注意: Codex 没有独立的 commands/ 目录，所有命令通过 Skills 实现。
#         项目规则写入项目根目录的 AGENTS.md。
# ============================================================
sync_codex() {
    log_title "同步 OpenAI Codex (.agents/)"
    local base="$PROJECT_ROOT/.agents"

    # Skills (from agent/)
    sync_dir "$AGENT_DIR/skills" "$base/skills" "  Skills → .agents/skills/"

    # Codex 没有独立的 commands/ 目录，Commands 作为 Skills 已同步
    log_info "  Commands 已通过 Skills 同步 (Codex 无独立 commands/ 目录)"

    # 生成/更新 AGENTS.md
    generate_agents_md
}

# ---------- 生成 AGENTS.md（Codex 项目规则/记忆） ----------
generate_agents_md() {
    local agents_file="$PROJECT_ROOT/AGENTS.md"
    local rules_dir="$AGENT_DIR/rules"

    if [[ ! -d "$rules_dir" ]]; then
        log_info "  AGENTS.md 跳过 (无 agent/rules/ 目录)"
        return
    fi

    log_info "  生成 AGENTS.md → $agents_file"

    {
        echo "# AGENTS.md"
        echo ""
        echo "> 本文件由 scripts/sync-agent-config.sh 自动生成"
        echo "> 生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
        echo ""
        echo "## 项目规则"
        echo ""

        # 拼接所有 rules 文件
        for f in "$rules_dir"/*.md; do
            [[ -f "$f" ]] || continue
            local name
            name="$(basename "$f" .md)"
            echo "### $name"
            echo ""
            cat "$f"
            echo ""
        done
    } > "$agents_file"

    log_info "  AGENTS.md 已更新"
}

# ---------- 帮助信息 ----------
show_help() {
    cat << EOF
用法: $0 [目标...]              (macOS / Linux / Git Bash / WSL)
     .\sync-agent-config.ps1    (Windows PowerShell 原生)

将 agent/ 目录下的 Skills、Commands、Rules 同步到各 AI 工具的标准配置目录。

Windows 用户:
  推荐: 安装 Git for Windows 后，在 Git Bash 中运行 bash sync-agent-config.sh
  备选: 在 PowerShell 中运行 .\sync-agent-config.ps1

目标:
  codebuddy    同步到 .codebuddy/   (CodeBuddy)
  workbuddy    同步到 .workbuddy/   (WorkBuddy)
  claude       同步到 .claude/      (Claude Code)
  codex        同步到 .agents/      (OpenAI Codex)
  all          同步所有目标 (默认)

示例:
  $0                    # 同步所有
  $0 codebuddy          # 仅同步 CodeBuddy
  $0 workbuddy          # 仅同步 WorkBuddy
  $0 claude codex       # 同步 Claude Code 和 Codex

源目录结构:
  agent/                # Agent 核心系统（可移植、自包含）
  ├── skills/           → 同步到各工具的 skills/ 目录
  ├── commands/         → 同步到各工具的 commands/ 目录
  ├── rules/            → 同步到各工具的 rules/ 目录
  ├── tools/            # Tool Schema（不同步，Agent 内部使用）
  ├── memory/           # 运行时记忆（不同步）
  ├── mcp/              # MCP 配置文档（不同步，手动管理）
  └── codebuddy-settings.json → .codebuddy/settings.json

目标目录结构:
  .codebuddy/ 与 .workbuddy/
  ├── skills/       ← agent/skills/*
  ├── commands/     ← agent/commands/*
  ├── rules/        ← agent/rules/*
  └── settings.json ← agent/codebuddy-settings.json

  .claude/
  ├── skills/       ← agent/skills/*
  ├── commands/     ← agent/commands/*
  └── rules/        ← agent/rules/*

  .agents/
  └── skills/       ← agent/skills/*

  AGENTS.md         ← agent/rules/* 拼接 (Codex 规则文件)
EOF
}

# ---------- 入口 ----------
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    show_help
    exit 0
fi

main "$@"
