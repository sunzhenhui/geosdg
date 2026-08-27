# Figure 4 — CAINFRA 基础设施 CA 框架（两状态 + 随机森林 + 竞争性斑块播种）

对应论文 Figure 4："The structure of the rule mining and simulation framework for infrastructure coverage."（及 Eq. 6–7）

```mermaid
flowchart TD
    X["Driving Factors X<br/>(physical · transport · location<br/>socio-economic · neighbourhood)"]:::mist --> RF
    RF["Random Forest (RF)<br/>P<sub>cover</sub>(x) = Σ I(h<sub>m</sub>(x)=d) / M<br/>(Eq. 6)"]:::primary --> PIK

    PIK{"Cell State k"}:::secondary
    PIK -->|"k = 1 · covered"| C1["P<sub>i,k</sub> = P<sub>cover</sub>(X<sub>i</sub>)<br/>(Eq. 7)"]:::mist
    PIK -->|"k = 0 · not covered"| C0["P<sub>i,k</sub> = 1 − P<sub>cover</sub>(X<sub>i</sub>)<br/>(Eq. 7)"]:::mist

    C1 --> TR["Transition rule (adapt Eq. 4)<br/>neighbourhood + probability"]:::mist
    C0 --> TR
    TR --> SEED["Competitive Patch-Seeding<br/>cell-level evolution"]:::accent
    SEED --> OUT["Future infrastructure<br/>coverage map"]:::navy

    subgraph NOTE["Anti-Circularity"]
        N1["Exclude POI categories that<br/>directly represent simulated<br/>infrastructure (e.g. transit stations)"]:::mist
    end

    classDef primary fill:#0A97D9,stroke:#0A97D9,color:#fff;
    classDef secondary fill:#56C02B,stroke:#56C02B,color:#fff;
    classDef accent fill:#E4572E,stroke:#E4572E,color:#fff;
    classDef navy fill:#0F2740,stroke:#0F2740,color:#fff;
    classDef mist fill:#F4F7FA,stroke:#CBD5E1,color:#16283A;
```

## 文字说明

CAINFRA 是土地利用 CA 的简化版，仅区分两种状态（覆盖 / 未覆盖）：

1. **驱动因子 X**：物理环境、交通、区位、社会经济、邻域交互等。
2. **随机森林估计覆盖率概率**：`P_cover(x)` 由 RF 各决策树投票得出（Eq. 6）。
3. **两状态判定**：`k=1`（覆盖）取 `P_cover`，`k=0`（未覆盖）取 `1−P_cover`（Eq. 7）。
4. **竞争性斑块播种**：用覆盖率概率替代 Eq. 4 中的开发概率，模拟基础设施覆盖的逐像元演化（Figure 4）。
5. **避免循环论证**：将直接代表被模拟基础设施的 POI 类别（如公交站点）从预测变量中剔除。
