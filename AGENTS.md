# AGENTS.md

> 本文件由 scripts/sync-agent-config.sh 自动生成
> 生成时间: 2026-07-24 17:13:21

## 项目规则

### coding-rules

# GeoSDG 编码规范

> 本文档定义了 GeoSDG 项目的 C++ 编码风格与开发规则。所有代码贡献者须遵循本文规范。

---

## 一、开发规则

### 1. 注释必须紧贴代码实体

- ✅ **注释和代码之间不能有空行**：注释必须直接位于类、方法或代码的上方
- ❌ **注释下有空行是错误的**：会导致注释和代码分离，影响可读性

**正确示例**：
```cpp
/**
 * @brief Calculate land proportion indicator
 * @param qstrFileName Path to land use data
 * @return 0 on success, non-zero on failure
 */
int calculateLandProportionIndicator(const std::string& qstrFileName);
```

**错误示例**：
```cpp
/**
 * @brief Calculate land proportion indicator
 */

int calculateLandProportionIndicator(const std::string& qstrFileName);
```

**适用范围**：
- 类注释
- 方法注释（Doxygen `@brief`、`@param`、`@return`、`@note`、`@warning`）
- 属性/成员变量注释
- 行内注释

### 2. Doxygen 文档注释规范

每个公共方法在 `.h` 中必须包含完整的 Doxygen 注释块，至少包含以下标签：

| 标签 | 说明 | 是否必需 |
|------|------|---------|
| `@brief` | 方法功能概述 | ✅ 必需 |
| `@param` | 参数说明，每个参数一个 | ✅ 必需 |
| `@return` | 返回值说明 | ✅ 必需 |
| `@note` | 重要注意事项 | 按需 |
| `@warning` | 已知问题或风险 | 按需（已知 bug 必须标注） |
| `@see` | 关联文档引用 | 按需 |

### 3. 代码区域分隔符

使用 `// ====...====` 风格的分隔线划分代码逻辑区域，可标注区域名称：

```cpp
// ============================================================================
// Public Interface
// ============================================================================

int CalculateSDG::calculateLandProportionIndicator(...)
{
    ...
}

// ============================================================================
// Private Helper Methods
// ============================================================================

double CalculateSDG::mean(double* pData, int nCount)
{
    ...
}
```

---

## 二、C++ 编码规范

### 1. 文件结构

每个 `.h` 文件以以下结构开始：

```cpp
/**
 * @file FileName.h
 * @brief Module description
 *
 * Detailed module description.
 */

#pragma once

#include <...>
```

- 使用 `#pragma once` 而非 include guard
- `@file` 注释块位于文件最顶部，`#pragma once` 紧随其后

### 2. 命名规范

#### 2.1 命名风格

| 类别 | 风格 | 示例 |
|------|------|------|
| 类名 | PascalCase | `CalculateSDG`、`ExtractPriorityAreas` |
| 方法名 | camelCase | `calculateLandProportionIndicator()`、`readLastCheckpoint()` |
| 私有方法 | camelCase | `mean()`、`variance()` |
| 命名空间宏 | 全大写 + 下划线 | `GEOSDG_PLATFORM_MACOS`、`LOG_INFO` |

#### 2.2 匈牙利前缀命名（局部变量与参数）

| 前缀 | 类型 | 示例 |
|------|------|------|
| `n` | `int` | `nCols`、`nRows`、`nCount` |
| `d` | `double` | `dResults`、`dMaxThreshold`、`dNodata` |
| `p` | 指针 | `poDS`、`pData` |
| `qstr` | `std::string` 参数 | `qstrFileName`、`qstrInputOriginal` |
| `b` | `bool` | `bState` |
| `v` | `std::vector` / 容器 | `vLUCCType`、`vLUCCFileNames` |
| `mqset` | `std::unordered_set` | `mqsetSelectLUCCTypes` |
| `i` | `int` 临时变量 | `iImgSizeX0` |

#### 2.3 成员变量

所有成员变量使用**尾部下划线** `_` 后缀：

```cpp
class Logger
{
private:
    std::ofstream logFile_;       // Log file stream
    std::string   logFilePath_;   // Log file path
    LogLevel      minLevel_;      // Minimum log level
};
```

### 3. 代码格式化

- **缩进**：4 空格（禁止 Tab）
- **大括号**：K&R 风格（左大括号不换行）
- **操作符**：两侧空格
- **长参数列表**：对齐换行

```cpp
int calculateSDG1131Indicator(const std::string& qstrLUCCFileName,
                               const std::string& qstrPopFileName,
                               const std::string& qstrOutput);
```

### 4. 错误处理

- 函数返回 `int`，0 表示成功，非0 表示失败
- 不抛出异常，通过 `Logger` 记录错误信息
- 所有除零操作必须显式保护：

```cpp
if (fabs(denominator) < 1e-15) {
    LOG_ERROR("Denominator is zero, skipping");
    return 1;
}
```

### 5. 日志输出规范

- **日志输出必须使用英文**：所有 `LOG_INFO`、`LOG_WARN`、`LOG_ERROR`、`LOG_DEBUG` 的消息内容必须是英文
- 日志级别使用规则：

| 级别 | 宏 | 用途 |
|------|-----|------|
| DEBUG | `LOG_DEBUG(...)` | 调试信息，用于开发排查 |
| INFO | `LOG_INFO(...)` | 正常运行的关键节点输出 |
| WARN | `LOG_WARN(...)` | 可恢复的异常情况 |
| ERROR | `LOG_ERROR(...)` | 导致操作失败的严重错误 |

```cpp
// ✅ 正确：英文日志
LOG_INFO("Calculating land proportion indicator for LUCC: " + qstrFileName);
LOG_ERROR("Failed to open input raster: " + qstrFileName);

// ❌ 错误：中文日志
LOG_INFO("正在计算土地比例指标：" + qstrFileName);
LOG_ERROR("无法打开输入栅格：" + qstrFileName);
```

### 6. GDAL 资源管理

- 入口调用 `GDALAllRegister()` + `CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO")`
- 打开的数据集必须对应 `GDALClose()`
- 优先以 `GA_ReadOnly` 方式打开输入文件，避免意外修改源数据
- 平台条件编译：

```cpp
#if !defined(GEOSDG_PLATFORM_WINDOWS)
    #include <cpl_conv.h>
#endif
```

### 7. 现代 C++ 特性使用

- 允许 `using namespace std;` 在 `.cpp` 中使用
- 使用 `std::filesystem`（C++17）处理文件路径
- 使用 `std::make_unique` 管理动态对象
- 使用 `std::chrono` 处理时间

---

## 三、Git 提交规范

### 1. 分支命名

- 新功能：`feature/功能简称`
- Bug 修复：`fix/问题简称`
- 文档更新：`docs/内容简称`

### 2. Commit Message 格式

```
<type>: <简短描述>

<详细说明（可选）>
```

Type 类型：

| Type | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档更新 |
| `refactor` | 代码重构 |
| `build` | 构建系统变更 |
| `chore` | 杂项 |

**示例**：
```
feat: 新增 macOS 平台支持

- CMakeLists.txt 新增平台检测与 GDAL 自适应引入
- main.cpp GDAL_DATA 运行时自动注入
- 移除废弃 GDAL 3.x 头文件引用
```

### 3. Commit 频率

- 每个逻辑变更点独立提交
- 避免"超级大 commit"：一个 commit 只做一件事
- 开发完成后使用 `git rebase -i` 合并微小提交

### doc-rules

# GeoSDG 文档管理规范

> 本文档定义了 GeoSDG 项目的文档编写、版本管理与变更跟踪标准。所有文档贡献者须遵循本文规范。

---

## 一、避免冗余文档

- 不在 README 中重复 API 文档的内容（API 文档应在头文件 Doxygen 中维护）
- 不在多个文档中维护相同信息（如 CLI 参数说明统一在 `geosdg-cli/README.md` 中）
- 代码即文档：优先通过清晰的命名和注释表达意图，减少额外的说明文档
- Skill 文档（`agent/skills/`）中的引用使用 `@see` 或超链接指向源文件，不复制粘贴代码

## 二、需求-方案文档分离

每个功能需求包含两个独立文档：

| 文档 | 文件名 | 内容 |
|------|--------|------|
| **需求文档** | `story.md` | 用户视角：需求目标、功能拆解、验收标准、边界情况 |
| **技术方案** | `design.md` | 实现视角：设计目标、架构图、模块设计、开发步骤、变更记录 |

`story.md` 通过 `→ wiki/features/xxx/design.md` 链接索引技术方案。

## 三、变更记录规范

### 什么时候需要记录变更？

✅ **需要记录**：
- 新增/删除公共 API
- 修改函数签名
- 修复头文件中标注的已知问题（`@warning`）
- 修改构建配置（CMakeLists.txt）
- 平台兼容性变更

❌ **不需要记录**：
- 代码格式调整（不影响逻辑）
- 注释修正（不影响 API）
- 添加/删除 `Logger` 调用
- 局部变量重命名

### 变更记录格式

在 `design.md` 末尾的 `📋 变更记录` 表格中记录，遵循 **3-5 行核心变更点** 原则：

```markdown
## 📋 变更记录

| 日期 | 版本 | 变更内容 | 变更人 |
|------|------|---------|--------|
| 2026-07-21 | v1.0 | 初始版本 | — |
| 2026-07-21 | v1.1 | 修复 SDG 13.2.2 排放系数原地修改问题 | — |
| 2026-07-22 | v1.2 | 新增 macOS 编译支持；CMake 平台自动适配 | — |
```

- 每条记录不超过一行，用分号分隔多项变更
- 版本号使用 `v主版本.次版本` 格式
- 变更内容使用中文简要描述

## 四、进度跟踪方式

- 开发步骤在 `design.md` 的 `📝 开发步骤` 表格中管理
- 每行包含：序号、步骤描述、预估工作量（小/中/大）、风险点、进度
- 进度使用 checkbox 标记（`✅` 已完成 / 待处理留空）

```markdown
| 序号 | 步骤 | 预估工作量 | 风险点 | 进度 |
|------|------|-----------|--------|------|
| 1 | CMakeLists.txt 新增平台宏定义 | 小 | 确保宏名不冲突 | ✅ |
| 2 | GDAL 引入改为平台分支 | 中 | find_package 版本兼容性 | ✅ |
| 3 | macOS 编译验证 | 小 | Homebrew GDAL 版本差异 |    |
```

## 五、需求文档模板（story.md）

```markdown
# 需求标题

## 📋 基本信息
| 项目 | 内容 |
|------|------|
| 类型 | CLI 功能补齐 / 新功能 / Bug 修复 |
| 提出日期 | YYYY-MM-DD |
| 完成日期 | YYYY-MM-DD |
| 状态 | ✅ 已完成 / 🔄 进行中 / 📋 待开始 |
| 优先级 | 高 / 中 / 低 |

## 🎯 需求目标
- [ ] 验收条目 1
- [ ] 验收条目 2

## 🧩 功能拆解
### 👤 用户使用
- 当前痛点
- 期望体验
- CLI 交互变化（表格）

### 关键行为
1. 行为描述

## ✅ 验收标准
- [ ] 验收条件 1

## ⚠️ 边界情况
| 情况 | 预期行为 |
|------|----------|

## 🔗 技术方案索引
→ `wiki/features/YYYY-MM-DD_story_xxx/design.md`
```

## 六、技术方案模板（design.md）

```markdown
# 技术方案：方案标题

## 📋 基本信息
| 项目 | 内容 |
|------|------|
| 需求文档 | `同目录/story.md` |
| 需求类型 | CLI 功能补齐 / 新功能 / Bug 修复 |

## 🎯 设计目标
- 目标 1
- 目标 2

## 🏗️ 整体架构
（ASCII art 架构图）

## 🧩 模块设计
### 涉及文件
| 文件 | 变更类型 | 说明 |
|------|---------|------|

### 变更点详解
（每个变更点单独说明）

## 📝 开发步骤
| 序号 | 步骤 | 预估工作量 | 风险点 | 进度 |
|------|------|-----------|--------|------|

## 📋 变更记录
| 日期 | 版本 | 变更内容 | 变更人 |
|------|------|---------|--------|
```

