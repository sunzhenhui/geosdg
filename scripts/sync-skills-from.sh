#!/usr/bin/env bash
# ============================================================
# sync-skills-from.sh
# 反向同步：将 AI 工具生成/云端拉回的 Skills 同步回 agent/skills/
# （agent/skills/ 是标准源头，供 sync-agent-config.sh 再分发到各工具）
#
# 与 sync-agent-config.sh 互为反向：
#   sync-agent-config.sh : agent/skills/  ──►  .codebuddy / .claude / .agents
#   sync-skills-from.sh  : 来源/skills    ──►  agent/skills/
#
# 关键点：同步前校验 Skill 标准格式（SKILL.md + YAML frontmatter 含
#        name/description），格式不合规的 Skill 会被跳过并报错，
#        避免不标准的 Skill 污染 agent/ 源头、导致再分发后无法安装。
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
AGENT_SKILLS_DIR="$PROJECT_ROOT/agent/skills"

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

# ---------- 配置 ----------
# 来源别名 → skills 子目录路径映射（兼容 bash 3.2，不用关联数组）
# 返回别名对应的 skills 相对路径；非别名返回空串
resolve_alias() {
    case "$1" in
        workbuddy) echo ".workbuddy/skills" ;;
        codebuddy) echo ".codebuddy/skills" ;;
        claude)    echo ".claude/skills" ;;
        agents)    echo ".agents/skills" ;;
        *)         echo "" ;;
    esac
}

DRY_RUN=false
FORCE=false
ONLY_SKILL=""

# ---------- Skill 标准格式校验 ----------
# 标准格式要求：
#   1. Skill 目录下存在 SKILL.md
#   2. SKILL.md 首行为 YAML frontmatter 起始 ---
#   3. frontmatter 内含 name: 与 description: 字段
# 返回 0 表示合规，非 0 表示不合规
validate_skill() {
    local skill_dir="$1"
    local skill_name
    skill_name="$(basename "$skill_dir")"
    local skill_md="$skill_dir/SKILL.md"

    if [[ ! -f "$skill_md" ]]; then
        log_error "  ✗ ${skill_name}: 缺少 SKILL.md"
        return 1
    fi

    # 首个非空行必须为 ---
    local first_line
    first_line="$(grep -m1 -v '^[[:space:]]*$' "$skill_md" || true)"
    if [[ "$first_line" != "---" ]]; then
        log_error "  ✗ ${skill_name}: SKILL.md 缺少 YAML frontmatter（首行应为 ---）"
        return 1
    fi

    # 提取 frontmatter 区块（第一对 --- 之间）
    local frontmatter
    frontmatter="$(awk 'NR==1&&/^---[[:space:]]*$/{f=1;next} f&&/^---[[:space:]]*$/{exit} f{print}' "$skill_md")"

    if ! grep -qE '^name:[[:space:]]*[^[:space:]]' <<< "$frontmatter"; then
        log_error "  ✗ ${skill_name}: frontmatter 缺少 name 字段"
        return 1
    fi
    if ! grep -qE '^description:[[:space:]]*[^[:space:]]' <<< "$frontmatter"; then
        log_error "  ✗ ${skill_name}: frontmatter 缺少 description 字段"
        return 1
    fi

    return 0
}

# ---------- 同步单个 Skill 目录 ----------
sync_one_skill() {
    local src_skill="$1"
    local dst_skill="$2"
    local skill_name
    skill_name="$(basename "$src_skill")"

    if $DRY_RUN; then
        log_info "  [dry-run] 将同步 ${skill_name} → agent/skills/${skill_name}/"
        return 0
    fi

    mkdir -p "$dst_skill"
    if command -v rsync &>/dev/null; then
        rsync -a --delete "$src_skill/" "$dst_skill/"
    else
        rm -rf "$dst_skill" && mkdir -p "$dst_skill"
        cp -R "$src_skill/"* "$dst_skill/" 2>/dev/null || true
    fi
    log_info "  ✓ ${skill_name} 已同步 → agent/skills/${skill_name}/"
}

# ---------- 主逻辑 ----------
run_sync() {
    local source_arg="$1"

    mkdir -p "$AGENT_SKILLS_DIR"

    # 累计统计（跨多个来源目录）
    TOTAL=0; VALID=0; SYNCED=0; SKIPPED=0

    # all: 遍历工程下所有存在的 IDE skills 目录
    if [[ "$source_arg" == "all" ]]; then
        log_title "反向同步 Skills：所有 IDE 目录 → agent/skills/"
        local any=false
        local a path abs
        for a in workbuddy codebuddy claude agents; do
            path="$(resolve_alias "$a")"
            abs="$PROJECT_ROOT/$path"
            if [[ -d "$abs" ]]; then
                any=true
                echo ""
                log_info "来源: $path"
                sync_from_dir "$abs"
            else
                log_warn "跳过不存在的来源: $path"
            fi
        done
        if ! $any; then
            log_error "未找到任何 IDE skills 目录 (.workbuddy/.codebuddy/.claude/.agents)"
            exit 1
        fi
    else
        # 单一来源：别名或自定义路径
        local src_dir
        local alias_path
        alias_path="$(resolve_alias "$source_arg")"
        if [[ -n "$alias_path" ]]; then
            src_dir="$PROJECT_ROOT/$alias_path"
        else
            if [[ "$source_arg" = /* ]]; then
                src_dir="$source_arg"
            else
                src_dir="$PROJECT_ROOT/$source_arg"
            fi
        fi

        log_title "反向同步 Skills：$src_dir → agent/skills/"

        if [[ ! -d "$src_dir" ]]; then
            log_error "来源目录不存在: $src_dir"
            exit 1
        fi
        sync_from_dir "$src_dir"
    fi

    echo ""
    log_title "同步汇总"
    echo -e "  发现 Skill : ${TOTAL}"
    echo -e "  格式合规   : ${VALID}"
    if $DRY_RUN; then
        echo -e "  预览模式   : 未写入任何文件"
    else
        echo -e "  已同步     : ${SYNCED}"
    fi
    echo -e "  已跳过     : ${SKIPPED}"

    if [[ $SKIPPED -gt 0 ]]; then
        echo ""
        log_warn "存在格式不合规的 Skill 未同步。标准格式要求："
        log_warn "  1) Skill 目录含 SKILL.md"
        log_warn "  2) SKILL.md 首行为 --- (YAML frontmatter)"
        log_warn "  3) frontmatter 含 name 与 description 字段"
    fi
    echo ""
    log_info "完成。可运行 scripts/sync-agent-config.sh 将 agent/skills/ 再分发到各工具。"
}

# ---------- 从单个来源目录同步（累加到全局统计 TOTAL/VALID/SYNCED/SKIPPED）----------
sync_from_dir() {
    local src_dir="$1"
    local src_skill skill_name dst_skill

    for src_skill in "$src_dir"/*/; do
        [[ -d "$src_skill" ]] || continue
        skill_name="$(basename "$src_skill")"

        # --only 过滤
        if [[ -n "$ONLY_SKILL" && "$skill_name" != "$ONLY_SKILL" ]]; then
            continue
        fi

        TOTAL=$((TOTAL + 1))
        dst_skill="$AGENT_SKILLS_DIR/$skill_name"

        # 格式校验
        if validate_skill "$src_skill"; then
            VALID=$((VALID + 1))
            sync_one_skill "$src_skill" "$dst_skill"
            $DRY_RUN || SYNCED=$((SYNCED + 1))
        else
            if $FORCE; then
                log_warn "  ⚠ ${skill_name}: 格式不合规，但 --force 已启用，仍执行同步"
                sync_one_skill "$src_skill" "$dst_skill"
                $DRY_RUN || SYNCED=$((SYNCED + 1))
            else
                log_warn "  → ${skill_name}: 已跳过（格式不合规，使用 --force 可强制同步）"
                SKIPPED=$((SKIPPED + 1))
            fi
        fi
    done
}

# ---------- 帮助信息 ----------
show_help() {
    cat << EOF
用法: $0 [来源] [选项]

反向同步：将 AI 工具生成/云端拉回的 Skills 同步回 agent/skills/
（agent/skills/ 是标准源头，供 sync-agent-config.sh 再分发到各工具）

来源（可用别名，或直接给出包含各 skill 子目录的路径）:
  workbuddy    从 .workbuddy/skills/  同步（默认）
  codebuddy    从 .codebuddy/skills/  同步
  claude       从 .claude/skills/     同步
  agents       从 .agents/skills/     同步
  all          遍历工程下所有存在的 IDE 目录依次同步
               (.workbuddy → .codebuddy → .claude → .agents，
                同名 Skill 以后者覆盖前者)
  <path>       自定义路径（相对项目根目录或绝对路径）

选项:
  --only <name>   仅同步指定名称的 Skill
  --dry-run       预览将同步的内容，不写入任何文件
  --force         即使格式不合规也强制同步（不推荐）
  -h, --help      显示本帮助

Skill 标准格式（与 agent/skills/ 现有格式一致）:
  <skill>/SKILL.md 首部必须为 YAML frontmatter，含 name 与 description：
    ---
    name: my-skill
    description: 简介 + 触发关键词
    ---
  说明：格式不标准的 Skill 同步到云端后，再用 sync-agent-config.sh
       分发会无法安装，故本脚本默认拒绝同步不合规 Skill。

示例:
  $0                       # 从 .workbuddy/skills 同步全部合规 Skill
  $0 codebuddy             # 从 .codebuddy/skills 同步
  $0 all                   # 从所有 IDE 目录读取并同步
  $0 all --dry-run         # 预览所有 IDE 目录
  $0 workbuddy --dry-run   # 预览
  $0 --only data-standardizer   # 仅同步 data-standardizer
  $0 ./some/dir/skills     # 从自定义路径同步
EOF
}

# ---------- 参数解析 ----------
SOURCE_ARG="workbuddy"
positional_set=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)  show_help; exit 0 ;;
        --dry-run)  DRY_RUN=true; shift ;;
        --force)    FORCE=true; shift ;;
        --only)     ONLY_SKILL="${2:-}"; [[ -z "$ONLY_SKILL" ]] && { log_error "--only 需要一个 Skill 名称"; exit 1; }; shift 2 ;;
        -*)         log_error "未知选项: $1"; show_help; exit 1 ;;
        *)          SOURCE_ARG="$1"; positional_set=true; shift ;;
    esac
done

run_sync "$SOURCE_ARG"
