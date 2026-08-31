"""L2-领域知识：模拟专家（CA + RF + Markov 情景模拟的"可信度守门人"）.

GeoSDG 的"未来维度"完全依赖 LUCC 情景模拟（元胞自动机 + 随机森林转换概率 +
Markov 需求预测）。任何 scenario_forecast / intervention_plan 类问题，其结论都
建立在"这次模拟到底可不可信"之上。本专家负责为模拟结果背书或亮红灯：
- 模拟精度（FoM / Kappa / OA）是否达标
- SSP1-5 情景假设与研究区是否匹配、外推年限是否过长
- RF 驱动因子是否充分、Markov 转移矩阵是否稳定
它只对"模拟本身的可信度"负责，不替 climate/planner 下结论。
"""

from __future__ import annotations

from typing import Any

from .base import ExpertBase


class SimulationExpert(ExpertBase):
    role = "simulation"
    layer = "L2"
    focus_indicators = ("SDG_11_3_1", "SDG_15_3_1", "SDG_13_2_2")
    system_prompt = (
        "You are a land-use change simulation analyst for GeoSDG. The platform "
        "projects future SDG indicators using Cellular Automata (CA) driven by "
        "Random-Forest transition probabilities and Markov land-demand under "
        "SSP1-5 scenarios. Your job is NOT to decide the substantive answer, but "
        "to CERTIFY whether a future/scenario projection is trustworthy: "
        "(1) simulation accuracy (Figure-of-Merit, Kappa, Overall Accuracy) vs "
        "acceptable thresholds; (2) whether the chosen SSP scenario fits the "
        "study region; (3) extrapolation horizon risk (the further out, the less "
        "reliable); (4) sufficiency of RF driving factors and stability of the "
        "Markov transition matrix. Downgrade confidence and raise concerns when "
        "the projection is being over-trusted."
    )
    output_schema = (
        '{"answer": "trust verdict on the projection (trustworthy/caution/unreliable)", '
        '"reason": "which accuracy/scenario/horizon factor drives the verdict", '
        '"evidence": [{"metric":"FoM","value":0.0,"threshold":0.0}], '
        '"confidence": 0.0-1.0, "concerns": ["..."]}'
    )

    def get_knowledge(self, indicators: list[str]) -> dict[str, Any]:
        """DKI 可调；骨架版返回 mock 的模拟精度与情景元数据."""
        _ = indicators
        return {
            "engine": "CA + RandomForest + Markov",
            "accuracy": {"figure_of_merit": 0.21, "kappa": 0.83, "overall_accuracy": 0.88},
            "accuracy_thresholds": {"figure_of_merit": 0.20, "kappa": 0.75},
            "scenarios_available": ["SSP1-RCP2.6", "SSP2-RCP4.5", "SSP5-RCP8.5"],
            "baseline_year": 2020,
            "target_years": [2030, 2060],
            "max_reliable_horizon_year": 2050,
        }
