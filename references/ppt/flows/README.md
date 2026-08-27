# GeoSDG 流程图 Mermaid 源码

> 本目录包含三张论文核心流程图的 Mermaid 源码，供接入外部模型（mermaid-cli、Mermaid Live、draw.io 等）生成 PNG/SVG。
>
> 对应论文：
> - `figure1-methodology.md` → 论文 Figure 1（GeoSDG 三大模块方法论总览）
> - `figure3-calculation.md` → 论文 Figure 3（指标计算范式）
> - `figure4-cainfra.md`   → 论文 Figure 4（CAINFRA 基础设施 CA 框架）

## 生成方式

### 方式一：mermaid-cli（本地）
```bash
npx -y @mermaid-js/mermaid-cli -i figure1-methodology.md -o figure1-methodology.png -b white -w 1600
```

### 方式二：Mermaid Live（在线）
1. 打开 https://mermaid.live
2. 粘贴对应 `.md` 中的 `mermaid` 代码块内容
3. 导出 PNG/SVG

### 配色约定（与 PPT 一致）
| 元素 | 色值 |
|------|------|
| 主色 primary | `#0A97D9` |
| 绿色 secondary | `#56C02B` |
| 橙色 accent | `#E4572E` |
| 深蓝 navy | `#0F2740` |
| 墨色 ink | `#16283A` |
