# Figure 1 — GeoSDG 方法论总览（三大模块）

对应论文 Figure 1："The GeoSDG toolkit consists of three essential steps…"

```mermaid
flowchart TD
    A["<b>GeoSDG Toolkit</b><br/>Integrated &amp; reusable framework"]:::navy --> B
    A --> C
    A --> D

    subgraph M1["Module 1 · Indicator Selection &amp; Quantification"]
        direction TB
        B["Select 10 SDGs / 17 targets<br/>→ 27 land-related indicators"]:::primary
        B1["4 measurement primitives:<br/>Proportion · Conversion · Buffer · Statistics"]:::mist
        B2["Normalize 0–100<br/>Weighted SDG score (Eq. 1–2)"]:::mist
        B --> B1 --> B2
    end

    subgraph M2["Module 2 · Spatiotemporal Dynamic Simulation &amp; Priority Areas"]
        direction TB
        C["CA-based simulation of<br/>land-use + infrastructure coverage"]:::secondary
        C1["CALUCC (multi-class land-use CA)"]:::mist
        C2["CAINFRA (two-state infrastructure CA)"]:::mist
        C3["Scoring + priority-area identification<br/>(4 categories, 4 levels)"]:::mist
        C --> C1 --> C2 --> C3
    end

    subgraph M3["Module 3 · Future Sustainable Development Prospects"]
        direction TB
        D["Iterative assessment of<br/>future SDG trajectories"]:::accent
        D1["Scenario projection 2020–2050<br/>under SSP scenarios"]:::mist
        D2["2030 &amp; 2050 targets<br/>policy priorities"]:::mist
        D --> D1 --> D2
    end

    M2 --> M3
    M1 --> M3

    classDef primary fill:#0A97D9,stroke:#0A97D9,color:#fff;
    classDef secondary fill:#56C02B,stroke:#56C02B,color:#fff;
    classDef accent fill:#E4572E,stroke:#E4572E,color:#fff;
    classDef navy fill:#0F2740,stroke:#0F2740,color:#fff;
    classDef mist fill:#F4F7FA,stroke:#CBD5E1,color:#16283A;
```

## 文字说明

GeoSDG 由三大模块构成，构成"评估 → 模拟 → 展望"的闭环：

1. **指标选择与量化**：从 UN SDGs 框架中筛选与土地动态最相关的 10 个目标、17 个子目标、27 个指标，按 4 类测量原语计算并归一化到 0–100。
2. **时空动态模拟与优先区识别**：用 CA 模型同时模拟土地利用（CALUCC）与基础设施覆盖（CAINFRA）变化，计算未来 SDG 得分并识别优先区（4 类、4 级）。
3. **未来可持续发展展望**：在 SSP 情景下投影 2020–2050 的 SDG 轨迹，支撑 2030 / 2050 目标与政策优先级。
