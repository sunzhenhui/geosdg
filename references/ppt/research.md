# GeoSDG 学术报告 — 内容素材

> 来源：用户提供资料（论文 + 已定稿大纲 geosdg-ppt-outline.md）
> 整理时间：2026-08-27
> 语言：英文（演示文稿为英文版）

---

## 0. 演示基本信息

- 会议主题：人工智能与大数据时代的 GIS（GIS in the Era of AI and Big Data）
- 报告切入点：面向可持续发展目标的 AI + GIS 空间化新路径
- 论文：Sun, Zhenhui, Minyi Gao, Mengya Li, Ziheng Xu, and Xia Li. 2026. "New Pathways for UN SDGs Spatialization: GeoSDG Toolkit Empowering the Sustainable Future under a Spatial Context." *International Journal of Geographical Information Science*, August, 1–24. doi:10.1080/13658816.2026.2717629
- 作者单位：School of Geographic Sciences, East China Normal University（华东师范大学地理科学学院）
- 建议时长：15–20 分钟；页数：23 页
- 目标听众：地理空间领域专家、政府部门代表、业界专业人士
- 一句话主线：以「AI 与大数据时代的 GIS」为背景，先讲清"为什么需要空间化 SDG"，再讲清 GeoSDG"怎么做"，然后用长三角案例证明"工具到底能做什么、边界在哪"，最后落到工具自身演进——从随论文发布的 GUI 走向面向 AI 时代的 AI Agent 形态。
- 叙事线：**背景 → 方法 → 案例 → 工具**（四段式）

---

## 1. 背景（Background）

### 1.1 SDG 的空间化挑战
- SDG 是应对全球资源环境挑战的通用框架：17 目标、169 子目标、230+ 指标。
- 诸多目标/指标**本质是空间的**，直接依赖土地利用与基础设施条件。
- 从全球议程落到区域行动，需要**空间显式、前瞻性**证据。
- 2030 期限临近、进度落后，学界提议框架**延伸至 2050**（2030 中期 + 2050 终期），要求**情景化预测**而非仅回顾性评估。

### 1.2 现有方法的局限（三类不足）
1. **回顾性（Retrospective）**：现有评分多基于历史统计，缺未来趋势与政策干预的动态模拟。
2. **非空间（Non-spatial）**：主流框架（iSDG、IFs、SDG-IO、T21 等）统计驱动为主，忽略空间变异与过程驱动的空间动态。
3. **单一要素（Single-domain）**：现有 CA 只模拟土地利用，未纳入对可达性/服务类指标关键的**基础设施动态**；尚无框架同时模拟二者并耦合空间 SDG 评估。

### 1.3 研究问题与三大贡献
- 一句话研究问题：如何将 SDG 空间化，从回顾性评估升级为前瞻、空间动态建模？
- 贡献 1：将 CA 引入 **10 个土地相关 SDG、27 指标**空间评估。
- 贡献 2：提出 **CAINFRA**，与土地利用并行模拟基础设施覆盖变化。
- 贡献 3：支持识别**关键时间窗口、区域差异与优先干预区**。

---

## 2. 方法（Method）

### 2.1 方法总览：三大模块（论文 Figure 1）
- (1) 指标选择与量化（Indicator selection & quantification）
- (2) 时空动态模拟、评分与优先区识别（Spatiotemporal simulation, scoring & priority-area identification）
- (3) 未来可持续发展展望（Future sustainability outlook）

### 2.2 指标系统：10 目标 27 指标（论文 Table 1）
- 10 SDG、17 target、27 指标；聚焦"土地相关"逻辑（参照城市体检、国土空间规划体检评估指南）。
- 按测量方法分四类：Land Proportion（土地占比）/ Land Conversion（土地转换）/ Buffer Zone（缓冲可达性）/ Total Statistics（综合统计）。

### 2.3 指标计算范式与归一化
- 四类原语覆盖 27 个指标（方法学工程化核心）。
- 归一化：正向 / 中性 / 负向 → 0–100。
- SDG 加权综合（式 1、式 2）：SDG score = Σ wᵢ · indicatorᵢ。

### 2.4 土地利用模拟 CALUCC
- 综合转换概率 = 随机森林整体概率 × 邻域效应 × 自适应惯性系数 × 约束因子 × 随机因子（式 3–5）。
- 轮盘赌选择（roulette-wheel）。
- "AI 驱动"具体含义：随机森林学习空间驱动因子到土地利用类型的转换规则。
- 自适应惯性系数平衡供给与需求。

### 2.5 基础设施模拟 CAINFRA
- 基础设施 CA（两状态：覆盖 / 未覆盖）。
- 随机森林估计覆盖概率 + 竞争性 patch-seeding（式 6–7、Figure 4）。
- 亮点：首次同时模拟土地利用和基础设施覆盖。

### 2.6 多尺度评估与优先区识别
- 栅格 CA 模拟 + 空间 SDG 得分。
- 多尺度聚合（城市群级 / 城市级）保证跨区域、跨分辨率可比（Figure 5）。
- 四类优先区 + 栅格叠加集成排序、四级分级 Low / Moderate / High / Critical（Figure 6）。

---

## 3. 案例（Case · 围绕工具价值，回扣论文 Discussion D1–D6）

### 3.0 论文 Discussion 六论点（工具价值锚点）
- **D1 前瞻性**：将回顾性评估扩展为前瞻、空间动态建模框架。
- **D2 双模拟**：CAINFRA 扩展传统土地利用 CA，土地 + 基础设施协同。
- **D3 强项与局限**：空间显式指标强（SDG 11/15, r>0.8），社会经济代理指标弱 →「补充而非替代」传统统计。
- **D4 模块化与可迁移**：模块化结构可适配，但可迁移性仅概念性（仅长三角验证）。
- **D5 可复现与多尺度**：识别何时/多强/是否均衡，三种时间模式（渐进改善/渐进下降/波动），多尺度区分整体与区域内部不均衡。
- **D6 情景依赖优先区**：优先区动态而非固定空间类别（方法学优势）。

### 3.1 研究区与数据：长三角（YRDUA）
- 211,700 km²、27 城市（沪苏浙皖）。
- 约占全国 GDP 1/5，发展热点。
- 数据清单：CNLUCC 30m 土地利用、高德 POI、OSM 道路、WorldPop 人口等（Table 2）。

### 3.2 工具价值① 算得准（精度验证）
- 页标题：How Accurate Is the Toolkit?
- CALUCC：FoM = 0.128、OA = 0.815；CAINFRA：FoM = 0.263、OA = 0.923。
- >70% 单元 FoM > 0.1、>90% OA > 0.8。
- 相关性：SDG 11（r = 0.898）、SDG 15（r = 0.831）强；SDG 3/4/6 中等；SDG 2/7/9 无显著。
- 回扣：D3 前半 + D1 —— 对直接关联土地覆盖/生态属性的空间显式指标表现尤其好。

### 3.3 工具价值② 看得远（前瞻模拟）
- 页标题：How Far Ahead Can It Look?
- 全部情景 SDG 总分上升；SSP1/2/5 到 2050 最高。
- SDG 6/7/13/14 强劲（>90）；SDG 2/15 持续下降；SDG 11 波动明显。
- 2030–2035 为关键波动期。
- 回扣：D1 + D5（时间模式）—— 识别出渐进改善、渐进下降、波动三种模式，并定位"何时"发生关键转变。

### 3.4 工具价值③ 看得细（多尺度异质）
- 页标题：How Fine-Grained Is the View?
- 2020 格局"中部高、南北低"。
- 2030 年 SSP2/4 改善最小、SSP3 失衡最大；2050 年 SSP4 区域差距最大。
- 回扣：D5（多尺度）—— 区分"整体区域表现"与"区域内部空间不均衡发展"。

### 3.5 工具价值④ 指得准（情景优先区）
- 页标题：Where Should We Intervene?
- 2030 年 SSP3 优先区最大（>15,000 km²）、SSP2 最小（8,667 km²）。
- 2050 年 SSP3 仍最大（22,853 km²）；优先区集中在苏南、浙北、皖东快速发展城市。
- 回扣：D6 —— 优先区位置/范围/构成随情景变化，是相对固定划定方法的方法学优势。

### 3.6 工具的边界（补充而非替代）
- 页标题：The Toolkit Is Not a Panacea
- D3 后半：对 SDG 2/7/9/13 弱相关，反映空间代理对复杂社会经济过程指标的局限 → 应补充而非替代传统统计监测。
- D4：模块化结构可适配到精细统计受限但地理数据可用的地区，但可迁移性目前仅概念性（仅长三角验证）；需本地数据 + 重新校准 + 适配指标/阈值/权重。

---

## 4. 工具演进（Tool Evolution）

### 4.1 版本演进总览
- v1.0（论文版）：Windows GUI 桌面程序（C++、GPL-3.0），随论文发布在 figshare（DOI:10.6084/m9.figshare.29445089.v3）。
- v2.0（CLI 版）：核心算法重构为独立、可编程命令行工具。
- v3.0（AI Agent 版）：配套 AI Agent Skill 体系，自然语言驱动（进行中）。
- 演进动机：论文版源码与 UI 受限于当时技术栈与设计目标，在 AI 时代已较难直接应用。

### 4.2 v1 论文 GUI 版
- Windows 桌面 GUI，C++ 实现，GPL-3.0 开源，含完整文档与源码，随论文附于 figshare。
- 局限（委婉）：强绑定 Windows、桌面交互范式、与 AI/自动化工作流衔接困难、复现与二次改造门槛较高。

### 4.3 v2 CLI 版
- 核心算法（指标计算、CA 模拟、优先区识别）重构为独立 CLI，跨平台、可脚本化、可复现。
- 基于 GDAL 栅格算子生态。
- 价值：方便集成到批处理、流水线与第三方系统。
- "可编程"是通往 AI 自动化的前提。

### 4.4 v3 AI Agent 版（落点）
- 在 CLI 之上构建 AI Agent Skill 体系，将计算能力封装为可被大模型调用的技能。
- 交互链路：用户自然语言提问 → 大模型理解意图 → 调用 Skill → 底层 CLI/GDAL 计算 → 返回结果与解读。
- 将 GeoSDG 从"科研工具"升级为"AI 时代的智能助手"，与会议主题最强呼应。

---

## 5. 结论与致谢（Conclusion & Acknowledgments）

### 5.1 结论与展望
- 三点结论：(1) 空间显式、前瞻性工具是 SDG 落地的关键；(2) GeoSDG 用 CA + SSP 实现土地与基础设施协同模拟与情景评估；(3) 从 GUI 到 CLI 再到 AI Agent，GeoSDG 正走向开放、可复现、智能化的新形态。
- 局限（诚实）：仅 27 指标、仅长三角验证、长期预测不确定。
- 展望：跨区域验证、指标扩展、更智能的 Agent 版本（进行中）。

### 5.2 致谢与参考文献
- 基金：国家自然科学基金重点项目 42130107。
- 合作者：Minyi Gao, Mengya Li, Ziheng Xu, Xia Li。
- 完整引用：Sun, Zhenhui, Minyi Gao, Mengya Li, Ziheng Xu, and Xia Li. 2026. "New Pathways for UN SDGs Spatialization: GeoSDG Toolkit Empowering the Sustainable Future under a Spatial Context." *International Journal of Geographical Information Science*, August, 1–24. doi:10.1080/13658816.2026.2717629.
- 数据与代码：figshare DOI:10.6084/m9.figshare.29445089.v3。

---

## 6. 图件素材清单

A 类（从论文提取）：Figure 1（Slide 6）、Figure 2（Slide 19）、Figure 3（Slide 8）、Figure 4（Slide 10）、Figure 5 & 6（Slide 11）、Figure 7 & 8（Slide 12）、Figure 9（Slide 14）、Figure 10（Slide 15）、Figure 11 & 12（Slide 16）、Table 1（Slide 7）、Table 2（Slide 12）、Table 3（Slide 13）。

B 类（需补充/新制，多数可纯 HTML/CSS 绘制占位）：封面主视觉（Slide 1）、SDG 图标墙（Slide 3）、时间轴 2015→2030→2050（Slide 3）、版本演进时间轴 v1→v2→v3（Slide 18）、CLI 命令示例（Slide 20）、AI Agent 交互 demo（Slide 21）、结论路线图（Slide 22）。

> 说明：本次 .pptx 导出中，B 类图件用 HTML/CSS 与 Lucide 图标绘制为占位视觉；A 类论文图件用带编号占位容器呈现，用户后续可替换为真实 PNG。
