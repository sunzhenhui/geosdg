#!/usr/bin/env bash
# ============================================================
# release.sh — GeoSDG 发布流程脚本
#
# 版本来源: git tag（非 CMakeLists.txt）
# 版本规范: Semantic Versioning  vX.Y.Z
#
# 用法:
#   ./scripts/release.sh                 # 自动检测版本级别并发布
#   ./scripts/release.sh auto            # 同上（显式指定 auto）
#   ./scripts/release.sh patch           # 强制 patch  (1.0.0 → 1.0.1)
#   ./scripts/release.sh minor           # 强制 minor  (1.0.0 → 1.1.0)
#   ./scripts/release.sh major           # 强制 major  (1.0.0 → 2.0.0)
#   ./scripts/release.sh current         # 显示当前版本 + 最新提交
#   ./scripts/release.sh list            # 列出所有 tag
#   ./scripts/release.sh log [N]         # 显示最近 N 条提交（默认 20）
#   ./scripts/release.sh notes [range]   # 生成 release notes
#   ./scripts/release.sh --dry-run ...   # 预演模式
#
# 自动检测规则（自上一个 tag 以来的 commit message）:
#   含 BREAKING CHANGE / major!  → major
#   含 feat:                     → minor
#   仅 fix:/docs:/chore: 等      → patch
#   无新提交                     → 不发布
#
# 流程:
#   1. 预检（工作区干净、在 main 分支）
#   2. 读取最新 tag → 当前版本
#   3. 分析 commit → 自动判定版本级别（或手动指定）
#   4. 计算新版本号
#   5. 同步 CMakeLists.txt（如存在）
#   6. 生成 CHANGELOG.md
#   7. 创建 release 分支 + 提交
#   8. 在 HEAD（最新提交）上打 annotated tag
#   9. 输出后续操作指引
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CMAKE_FILE="$PROJECT_ROOT/geosdg-cli/CMakeLists.txt"

# ---------- 颜色输出 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

log_info()    { echo -e "${GREEN}[INFO]${NC}  $1"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC}  $1"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $1"; }
log_step()    { echo -e "\n${CYAN}▸${NC} $1"; }
log_success() { echo -e "${GREEN}✓${NC} $1"; }
log_title()   { echo -e "\n${CYAN}━━━ $1 ━━━${NC}"; }

# ---------- 全局标志 ----------
DRY_RUN=false

# ============================================================
# 版本读取 — 从 git tag 获取
# ============================================================

# 获取最新的版本 tag（v 开头，符合 X.Y.Z 或 X.Y 格式）
get_latest_tag() {
    git tag -l 'v[0-9]*' --sort=-version:refname 2>/dev/null | head -1
}

# 从 tag 提取纯版本号（去掉 v 前缀）
get_version_from_tag() {
    local tag="$1"
    echo "$tag" | sed -E 's/^v//'
}

# 获取当前版本（三位格式 X.Y.Z）
read_current_version() {
    local tag
    tag="$(get_latest_tag)"

    if [[ -z "$tag" ]]; then
        # 没有 tag，尝试从 CMakeLists.txt 读初始版本
        if [[ -f "$CMAKE_FILE" ]]; then
            local cmake_ver
            cmake_ver="$(sed -nE 's/^project\(geosdg-cli VERSION ([0-9]+\.[0-9]+(\.[0-9]+)?).*/\1/p' "$CMAKE_FILE" | head -1)"
            if [[ -n "$cmake_ver" ]]; then
                # 补全三位
                [[ "$cmake_ver" =~ ^[0-9]+\.[0-9]+$ ]] && cmake_ver="${cmake_ver}.0"
                echo "$cmake_ver"
                return
            fi
        fi
        echo "0.1.0"
        return
    fi

    local version
    version="$(get_version_from_tag "$tag")"

    # 补全三位
    [[ "$version" =~ ^[0-9]+\.[0-9]+$ ]] && version="${version}.0"

    echo "$version"
}

# ============================================================
# 版本计算
# ============================================================

bump_version() {
    local current="$1"
    local bump_type="$2"

    IFS='.' read -r major minor patch <<< "$current"
    patch="${patch:-0}"

    case "$bump_type" in
        major) major=$((major + 1)); minor=0; patch=0 ;;
        minor) minor=$((minor + 1)); patch=0 ;;
        patch) patch=$((patch + 1)) ;;
        *)
            log_error "Unknown bump type: $bump_type"
            exit 1
            ;;
    esac

    echo "${major}.${minor}.${patch}"
}

# ============================================================
# Commit 分析 — 自动判定版本级别
# ============================================================

# 获取自上一个 tag 以来的 commit range
get_commit_range() {
    local last_tag
    last_tag="$(get_latest_tag)"

    if [[ -n "$last_tag" ]]; then
        echo "${last_tag}..HEAD"
    else
        # 没有 tag，取所有提交
        echo "HEAD"
    fi
}

# 统计自上一个 tag 以来的提交数
count_commits_since_tag() {
    local range
    range="$(get_commit_range)"
    git log "$range" --oneline 2>/dev/null | wc -l | tr -d ' '
}

# 分析 commit message，自动判定版本级别
# 返回: major | minor | patch | none
detect_bump_type() {
    local range
    range="$(get_commit_range)"

    local commit_count
    commit_count="$(git log "$range" --oneline 2>/dev/null | wc -l | tr -d ' ')"

    if [[ "$commit_count" -eq 0 ]]; then
        echo "none"
        return
    fi

    # 检查 BREAKING CHANGE 或 major! 标记
    # 搜索 commit message body 中的 BREAKING CHANGE
    local breaking_count
    breaking_count="$(git log "$range" --pretty=format:'%B' 2>/dev/null | grep -ciE 'BREAKING[ -]CHANGE|major!' || true)"
    if [[ "$breaking_count" -gt 0 ]]; then
        echo "major"
        return
    fi

    # 检查 feat: 前缀
    local feat_count
    feat_count="$(git log "$range" --pretty=format:'%s' 2>/dev/null | grep -ciE '^feat(\(.+\))?:' || true)"
    if [[ "$feat_count" -gt 0 ]]; then
        echo "minor"
        return
    fi

    # 默认 patch
    echo "patch"
}

# 显示自上一个 tag 以来的提交摘要
show_commits_since_tag() {
    local range
    range="$(get_commit_range)"

    local last_tag
    last_tag="$(get_latest_tag)"

    if [[ -n "$last_tag" ]]; then
        echo -e "  Since tag:      ${BOLD}${last_tag}${NC}"
    else
        echo -e "  Since tag:      ${DIM}(none — all commits)${NC}"
    fi

    local total
    total="$(git log "$range" --oneline 2>/dev/null | wc -l | tr -d ' ')"
    echo -e "  New commits:    ${BOLD}${total}${NC}"

    if [[ "$total" -eq 0 ]]; then
        echo -e "  ${YELLOW}No new commits since last tag. Nothing to release.${NC}"
        return
    fi

    # 按类型统计
    local feat fix docs refactor chore other
    feat="$(git log "$range" --pretty=format:'%s' 2>/dev/null | grep -ciE '^feat(\(.+\))?:' || true)"
    fix="$(git log "$range" --pretty=format:'%s' 2>/dev/null | grep -ciE '^fix(\(.+\))?:' || true)"
    docs="$(git log "$range" --pretty=format:'%s' 2>/dev/null | grep -ciE '^docs(\(.+\))?:' || true)"
    refactor="$(git log "$range" --pretty=format:'%s' 2>/dev/null | grep -ciE '^refactor(\(.+\))?:' || true)"
    chore="$(git log "$range" --pretty=format:'%s' 2>/dev/null | grep -ciE '^chore(\(.+\))?:' || true)"
    other=$((total - feat - fix - docs - refactor - chore))

    echo -e "  Breakdown:      feat=${feat}  fix=${fix}  docs=${docs}  refactor=${refactor}  chore=${chore}  other=${other}"

    # 检测到的版本级别
    local detected
    detected="$(detect_bump_type)"
    local bump_label
    case "$detected" in
        major) bump_label="${RED}major${NC}" ;;
        minor) bump_label="${GREEN}minor${NC}" ;;
        patch) bump_label="${YELLOW}patch${NC}" ;;
        none)  bump_label="${DIM}none${NC}" ;;
    esac
    echo -e "  Detected bump:  ${bump_label}"
}

# ============================================================
# Release Notes 生成
# ============================================================

generate_release_notes() {
    local new_version="$1"
    local range="$2"

    if [[ -z "$range" ]]; then
        range="$(get_commit_range)"
    fi

    local last_tag
    last_tag="$(get_latest_tag)"

    echo ""
    echo "## v${new_version} ($(date '+%Y-%m-%d'))"
    echo ""

    # 按类型分组
    local feat_commits fix_commits other_commits

    feat_commits="$(git log "$range" --pretty=format:'- %s' --grep='^feat' 2>/dev/null || true)"
    fix_commits="$(git log "$range" --pretty=format:'- %s' --grep='^fix' 2>/dev/null || true)"
    other_commits="$(git log "$range" --pretty=format:'- %s' --invert-grep --grep='^feat' --invert-grep --grep='^fix' 2>/dev/null || true)"

    if [[ -n "$feat_commits" ]]; then
        echo "### ✨ New Features"
        echo "$feat_commits"
        echo ""
    fi

    if [[ -n "$fix_commits" ]]; then
        echo "### 🐛 Bug Fixes"
        echo "$fix_commits"
        echo ""
    fi

    if [[ -n "$other_commits" ]]; then
        echo "### 📦 Other Changes"
        echo "$other_commits"
        echo ""
    fi

    local total_commits
    total_commits="$(git log "$range" --oneline 2>/dev/null | wc -l | tr -d ' ')"
    local base
    if [[ -n "$last_tag" ]]; then
        base="$last_tag"
    else
        base="initial commit"
    fi
    echo "> ${total_commits} commits since ${base}"
}

# ============================================================
# CMakeLists.txt 同步（可选）
# ============================================================

sync_cmake_version() {
    local new_version="$1"

    if [[ ! -f "$CMAKE_FILE" ]]; then
        return
    fi

    # 读取 CMake 中的当前版本
    local cmake_ver
    cmake_ver="$(sed -nE 's/^project\(geosdg-cli VERSION ([0-9]+\.[0-9]+(\.[0-9]+)?).*/\1/p' "$CMAKE_FILE" | head -1)"

    if [[ -z "$cmake_ver" ]]; then
        log_warn "Could not parse version from CMakeLists.txt, skipping sync"
        return
    fi

    if [[ "$cmake_ver" == "$new_version" ]]; then
        log_info "CMakeLists.txt already at v${new_version}, no update needed"
        return
    fi

    if $DRY_RUN; then
        log_info "[DRY-RUN] Would update CMakeLists.txt: ${cmake_ver} → ${new_version}"
        return
    fi

    sed -i.bak "s/project(geosdg-cli VERSION ${cmake_ver}/project(geosdg-cli VERSION ${new_version}/" "$CMAKE_FILE"
    rm -f "${CMAKE_FILE}.bak"
    log_success "CMakeLists.txt synced: ${cmake_ver} → ${new_version}"
}

# ============================================================
# 预检
# ============================================================

preflight_check() {
    log_title "Pre-flight Check"

    # git 仓库
    if ! git rev-parse --is-inside-work-tree &>/dev/null; then
        log_error "Not inside a git repository"
        exit 1
    fi
    log_success "Git repository detected"

    # 工作区干净
    if ! git diff --quiet HEAD 2>/dev/null || ! git diff --cached --quiet 2>/dev/null; then
        log_error "Working tree has uncommitted changes:"
        git status --short
        exit 1
    fi
    log_success "Working tree is clean"

    # 分支检查
    local current_branch
    current_branch="$(git branch --show-current)"
    if [[ "$current_branch" != "main" && "$current_branch" != "master" ]]; then
        log_warn "Not on main/master (current: $current_branch)"
        log_warn "Releases should be cut from main. Continue? [y/N]"
        read -r confirm
        [[ "$confirm" =~ ^[Yy]$ ]] || { log_info "Aborted"; exit 0; }
    else
        log_success "On $current_branch branch"
    fi

    # 未推送的提交
    local ahead
    ahead="$(git rev-list --count '@{upstream}..HEAD' 2>/dev/null || echo "0")"
    if [[ "$ahead" -gt 0 ]]; then
        log_warn "Branch is $ahead commit(s) ahead of upstream — consider pushing first"
    fi

    # HEAD commit 信息
    local head_sha head_msg head_date
    head_sha="$(git rev-parse --short HEAD)"
    head_msg="$(git log -1 --format='%s' HEAD)"
    head_date="$(git log -1 --format='%ci' HEAD | cut -d' ' -f1)"
    echo -e "  HEAD:           ${BOLD}${head_sha}${NC} ${DIM}${head_date}${NC}"
    echo -e "  Message:        ${head_msg}"
}

# ============================================================
# 主发布流程
# ============================================================

do_release() {
    local bump_type="${1:-auto}"

    preflight_check

    # ---- 读取当前版本 ----
    local current_version
    current_version="$(read_current_version)"

    local latest_tag
    latest_tag="$(get_latest_tag)"

    # ---- 检查是否有新提交 ----
    local new_commits
    new_commits="$(count_commits_since_tag)"

    if [[ "$new_commits" -eq 0 ]]; then
        log_error "No new commits since ${latest_tag:-start}. Nothing to release."
        exit 0
    fi

    # ---- 自动检测版本级别 ----
    local detected_type
    detected_type="$(detect_bump_type)"

    if [[ "$bump_type" == "auto" ]]; then
        bump_type="$detected_type"
        log_info "Auto-detected bump type: ${bump_type}"
    else
        log_info "Manual bump type: ${bump_type} (detected: ${detected_type})"
    fi

    # ---- 计算新版本 ----
    local new_version
    new_version="$(bump_version "$current_version" "$bump_type")"

    local tag_name="v${new_version}"
    local release_branch="release/v${new_version}"
    local head_sha
    head_sha="$(git rev-parse --short HEAD)"

    # ---- 展示发布计划 ----
    log_title "Release Plan"

    if [[ -n "$latest_tag" ]]; then
        echo -e "  Current tag:    ${BOLD}${latest_tag}${NC} → version ${current_version}"
    else
        echo -e "  Current tag:    ${DIM}(none)${NC} → version ${current_version}"
    fi

    echo -e "  New version:    ${BOLD}${new_version}${NC}"
    echo -e "  Bump type:      ${BOLD}${bump_type}${NC} ${DIM}(detected: ${detected_type})${NC}"
    echo -e "  New tag:        ${BOLD}${tag_name}${NC}"
    echo -e "  Target commit:  ${BOLD}${head_sha}${NC} ${DIM}$(git log -1 --format='%s' HEAD)${NC}"
    echo -e "  Release branch: ${BOLD}${release_branch}${NC}"
    echo -e "  Dry run:        ${DRY_RUN}"

    echo ""
    show_commits_since_tag

    # ---- 确认 ----
    echo ""
    log_warn "Proceed with release v${new_version} on commit ${head_sha}? [y/N]"
    read -r confirm
    [[ "$confirm" =~ ^[Yy]$ ]] || { log_info "Aborted"; exit 0; }

    # ---- Step 1: 同步 CMakeLists.txt ----
    log_step "Step 1/5: Sync CMakeLists.txt"
    sync_cmake_version "$new_version"

    # ---- Step 2: 生成 Release Notes + CHANGELOG ----
    log_step "Step 2/5: Generate CHANGELOG.md"

    local release_notes
    release_notes="$(generate_release_notes "$new_version" "")"

    if $DRY_RUN; then
        log_info "[DRY-RUN] Release notes preview:"
        echo "$release_notes"
    else
        local changelog="$PROJECT_ROOT/CHANGELOG.md"
        if [[ -f "$changelog" ]]; then
            local tmp
            tmp="$(mktemp)"
            { echo "$release_notes"; echo ""; cat "$changelog"; } > "$tmp"
            mv "$tmp" "$changelog"
            log_success "CHANGELOG.md updated (prepended v${new_version})"
        else
            echo "$release_notes" > "$changelog"
            log_success "CHANGELOG.md created"
        fi
    fi

    # ---- Step 3: 创建 release 分支 + 提交 ----
    log_step "Step 3/5: Create release branch and commit"

    if $DRY_RUN; then
        log_info "[DRY-RUN] Would create branch: $release_branch"
        log_info "[DRY-RUN] Would commit: CMakeLists.txt, CHANGELOG.md"
    else
        if git show-ref --verify --quiet "refs/heads/$release_branch"; then
            log_error "Branch $release_branch already exists"
            exit 1
        fi

        git checkout -b "$release_branch"

        # 暂存版本相关文件
        local files_to_add=()
        [[ -f "$CMAKE_FILE" ]] && files_to_add+=("$CMAKE_FILE")
        [[ -f "$PROJECT_ROOT/CHANGELOG.md" ]] && files_to_add+=("$PROJECT_ROOT/CHANGELOG.md")

        if [[ ${#files_to_add[@]} -gt 0 ]]; then
            git add "${files_to_add[@]}"
            git commit -m "release: v${new_version}

- Bump version: ${current_version} → ${new_version}
- Update CHANGELOG.md
- Tagged on commit ${head_sha}"
            log_success "Committed version changes on $release_branch"
        else
            log_info "No files to commit"
        fi
    fi

    # ---- Step 4: 在 HEAD 上打 tag ----
    log_step "Step 4/5: Create annotated tag on HEAD"

    if $DRY_RUN; then
        log_info "[DRY-RUN] Would create tag: ${tag_name} on ${head_sha}"
    else
        if git tag -l "$tag_name" | grep -q .; then
            log_error "Tag $tag_name already exists"
            exit 1
        fi

        # tag 打在 release 分支的 HEAD 上（包含版本提交）
        local tag_message
        tag_message="$(echo "$release_notes" | sed 's/^##[^ ]* //; s/^###.*//; s/^> //' | grep -v '^$' | head -20)"

        git tag -a "$tag_name" -m "Release ${tag_name}

${tag_message}"

        local tag_sha
        tag_sha="$(git rev-parse --short "$tag_name")"
        log_success "Tag created: ${tag_name} → ${tag_sha}"
    fi

    # ---- Step 5: 后续指引 ----
    log_step "Step 5/5: Next steps"

    echo ""
    echo -e "${BOLD}Release ${tag_name} is ready locally!${NC}"
    echo ""
    echo -e "  ${CYAN}# Push release branch + tag to remote${NC}"
    echo "  git push origin ${release_branch}"
    echo "  git push origin ${tag_name}"
    echo ""
    echo -e "  ${CYAN}# Merge back to main${NC}"
    echo "  git checkout main"
    echo "  git merge ${release_branch}"
    echo "  git push origin main"
    echo ""
    echo -e "  ${CYAN}# Or create a PR: ${release_branch} → main${NC}"
    echo ""
    echo -e "  ${CYAN}# Create a GitHub/GitLab Release from tag ${tag_name}${NC}"
    echo "  # Copy release notes from CHANGELOG.md"
    echo ""

    if ! $DRY_RUN; then
        log_warn "Currently on branch: ${release_branch}"
        log_warn "Switch back: git checkout main"
    fi
}

# ============================================================
# 子命令
# ============================================================

# 显示当前版本信息
show_current() {
    local version
    version="$(read_current_version)"

    local latest_tag
    latest_tag="$(get_latest_tag)"

    local head_sha head_date head_msg
    head_sha="$(git rev-parse --short HEAD)"
    head_date="$(git log -1 --format='%ci' HEAD | cut -d' ' -f1)"
    head_msg="$(git log -1 --format='%s' HEAD)"

    echo -e "Current version:  ${BOLD}v${version}${NC}"

    if [[ -n "$latest_tag" ]]; then
        local tag_sha tag_date
        tag_sha="$(git rev-parse --short "$latest_tag")"
        tag_date="$(git log -1 --format='%ci' "$latest_tag" | cut -d' ' -f1)"
        echo -e "Latest tag:       ${BOLD}${latest_tag}${NC} ${DIM}(${tag_sha} ${tag_date})${NC}"

        local ahead
        ahead="$(git rev-list --count "${latest_tag}..HEAD" 2>/dev/null || echo "?")"
        echo -e "Commits since:    ${BOLD}${ahead}${NC}"
    else
        echo -e "Latest tag:       ${DIM}(none)${NC}"
    fi

    echo -e "HEAD commit:      ${BOLD}${head_sha}${NC} ${DIM}${head_date}${NC}"
    echo -e "HEAD message:     ${head_msg}"

    # 显示自动检测结果
    if [[ -n "$latest_tag" ]] || true; then
        local detected
        detected="$(detect_bump_type)"
        if [[ "$detected" != "none" ]]; then
            local next_version
            next_version="$(bump_version "$version" "$detected")"
            echo ""
            echo -e "Auto-detected:    ${BOLD}${detected}${NC} → next would be ${BOLD}v${next_version}${NC}"
        fi
    fi
}

# 列出所有 tag
list_tags() {
    local tags
    tags="$(git tag -l 'v[0-9]*' --sort=version:refname)"

    if [[ -z "$tags" ]]; then
        echo "No version tags found."
        return
    fi

    printf "%-12s  %-10s  %-8s  %s\n" "TAG" "DATE" "SHA" "MESSAGE"
    printf "%-12s  %-10s  %-8s  %s\n" "---" "----" "---" "-------"

    echo "$tags" | while read -r tag; do
        local date sha msg
        date="$(git log -1 --format='%cs' "$tag" 2>/dev/null)"
        sha="$(git rev-parse --short "$tag" 2>/dev/null)"
        msg="$(git log -1 --format='%s' "$tag" 2>/dev/null | cut -c1-50)"
        printf "%-12s  %-10s  %-8s  %s\n" "$tag" "$date" "$sha" "$msg"
    done
}

# 显示最近提交
show_log() {
    local count="${1:-20}"
    local range
    range="$(get_commit_range)"

    local last_tag
    last_tag="$(get_latest_tag)"

    echo -e "Commits since ${BOLD}${last_tag:-start}${NC}:"
    echo ""
    git log "$range" --oneline --graph --decorate -"$count" 2>/dev/null
}

# ============================================================
# 帮助
# ============================================================

show_help() {
    cat << EOF
Usage: $0 [options] [command]

GeoSDG Release Management Script

Version source: git tags (vX.Y.Z)
Auto-detection: analyzes commit messages since last tag

Commands:
  (default)            Auto-detect bump type and release
  auto                 Same as default (explicit)
  patch                Force patch bump  (1.0.0 → 1.0.1)
  minor                Force minor bump  (1.0.0 → 1.1.0)
  major                Force major bump  (1.0.0 → 2.0.0)
  current              Show current version, latest tag, HEAD commit
  list                 List all version tags with details
  log [N]              Show recent N commits since last tag (default 20)
  notes [range]        Generate release notes (default: since last tag)

Options:
  --dry-run            Preview without making any changes
  -h, --help           Show this help message

Auto-detection rules (commits since last tag):
  BREAKING CHANGE / major!  →  major bump
  feat:                     →  minor bump
  fix: / docs: / chore:     →  patch bump
  no new commits            →  abort

Examples:
  $0                           # Auto-detect and release
  $0 --dry-run                 # Preview auto-detected release
  $0 --dry-run minor           # Preview forced minor release
  $0 current                   # Check current state
  $0 list                      # List all tags
  $0 log 10                    # Show last 10 commits
  $0 notes v1.0.0..HEAD        # Notes for specific range

Release flow:
  1. Pre-flight (clean tree, on main)
  2. Read latest tag → current version
  3. Analyze commits → detect bump type
  4. Calculate new version
  5. Sync CMakeLists.txt (if exists)
  6. Generate CHANGELOG.md
  7. Create release/vX.Y.Z branch + commit
  8. Tag HEAD with annotated tag vX.Y.Z
  9. Print push/merge instructions
EOF
}

# ============================================================
# 入口
# ============================================================

if [[ $# -eq 0 ]]; then
    # 无参数 = auto
    do_release "auto"
    exit 0
fi

# 解析选项
args=()
for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=true ;;
        -h|--help) show_help; exit 0 ;;
        *) args+=("$arg") ;;
    esac
done

if [[ ${#args[@]} -eq 0 ]]; then
    do_release "auto"
    exit 0
fi

command="${args[0]}"

cd "$PROJECT_ROOT"

case "$command" in
    auto)
        do_release "auto"
        ;;
    patch|minor|major)
        do_release "$command"
        ;;
    current)
        show_current
        ;;
    list)
        list_tags
        ;;
    log)
        show_log "${args[1]:-}"
        ;;
    notes)
        generate_release_notes "$(read_current_version)" "${args[1]:-}"
        ;;
    *)
        log_error "Unknown command: $command"
        echo ""
        show_help
        exit 1
        ;;
esac
