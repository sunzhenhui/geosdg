"""Moderator：多专家合议编排 + 证据合并.

流程（骨架版，还没加二轮争辩，那是"深度设计"档的事）：
  1) 按 routing.py 决定本题需要哪些专家
  2) 逐个调用专家 → 收集 ExpertOpinion
  3) 合并：按 confidence 加权投票 + 汇总 concerns + 拼 evidence 链
  4) 返回主 answer + 全部 opinions 明细（PEDA 拿去序列化）
"""

from __future__ import annotations

from collections import Counter
from typing import Any

from ..utils import common
from .base import ExpertBase, ExpertOpinion
from .climate_expert import ClimateExpert
from .data_quality_expert import DataQualityExpert
from .devil_advocate_expert import DevilAdvocateExpert
from .ecologist_expert import EcologistExpert
from .economist_expert import EconomistExpert
from .planner_expert import PlannerExpert
from .policy_expert import PolicyExpert
from .routing import group_by_layer
from .simulation_expert import SimulationExpert
from .sociologist_expert import SociologistExpert
from .statistician_expert import StatisticianExpert
from .un_expert import UnExpert

# ============================================================================
# 专家注册表：role → 实例
# ============================================================================

def _build_registry() -> dict[str, ExpertBase]:
    experts: list[ExpertBase] = [
        PlannerExpert(),
        EcologistExpert(),
        EconomistExpert(),
        SociologistExpert(),
        UnExpert(),
        PolicyExpert(),
        ClimateExpert(),
        DataQualityExpert(),
        SimulationExpert(),
        DevilAdvocateExpert(),
        StatisticianExpert(),
    ]
    return {e.role: e for e in experts}


class Moderator:
    """会议主持人：召集专家、收集意见、合议裁决."""

    def __init__(self, registry: dict[str, ExpertBase] | None = None) -> None:
        self.registry = registry or _build_registry()

    # ------------------------------------------------------------------
    # 主入口
    # ------------------------------------------------------------------

    def deliberate(
        self,
        task: str,
        task_type: str,
        sdg_meta: dict[str, Any],
        knowledge: dict[str, Any],
    ) -> dict[str, Any]:
        """跑一轮合议，返回结构化裁决结果."""
        plan = group_by_layer(task_type)

        opinions: list[ExpertOpinion] = []
        for layer in ("L1", "L2", "L3"):
            for role in plan.get(layer, []):
                expert = self.registry.get(role)
                if expert is None:
                    if common.echo:
                        print(f"[moderator] WARN unknown expert role: {role}")
                    continue
                op = expert.assess(task, task_type, sdg_meta, knowledge)
                opinions.append(op)
                if common.echo:
                    print(f"[moderator] {op.role:>16} ({op.layer}) → conf={op.confidence:.2f}")

        return self._merge(opinions, task_type)

    # ------------------------------------------------------------------
    # 合议逻辑
    # ------------------------------------------------------------------

    def _merge(
        self,
        opinions: list[ExpertOpinion],
        task_type: str,
    ) -> dict[str, Any]:
        """把 N 份 ExpertOpinion 合并成一份最终答案.

        规则（骨架版）：
        - L1 意见做置信度加权投票，产出 `answer`
        - L2 意见汇总为 `constraints`（红线/UN 口径/数据质量）
        - L3 意见汇总为 `challenges`（反方观点 + 统计校准）
        - 汇总所有 `evidence` 与 `concerns`
        """
        l1 = [o for o in opinions if o.layer == "L1"]
        l2 = [o for o in opinions if o.layer == "L2"]
        l3 = [o for o in opinions if o.layer == "L3"]

        main_answer = self._weighted_vote([o.answer for o in l1],
                                          [o.confidence for o in l1])

        overall_conf = self._aggregate_confidence(opinions)

        return {
            "task_type": task_type,
            "answer": main_answer,
            "confidence": overall_conf,
            "reasoning": {
                "core_views": [self._brief(o) for o in l1],
                "constraints": [self._brief(o) for o in l2],
                "challenges": [self._brief(o) for o in l3],
            },
            "evidence": [ev for o in opinions for ev in o.evidence],
            "concerns": [c for o in opinions for c in o.concerns],
            "experts_invoked": [o.role for o in opinions],
        }

    # ------------------------------------------------------------------
    # helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _weighted_vote(answers: list[Any], weights: list[float]) -> Any:
        """置信度加权投票；answer 不可 hash 时退化为 first-wins."""
        if not answers:
            return None
        try:
            counter: Counter[Any] = Counter()
            for a, w in zip(answers, weights):
                counter[a] += w
            return counter.most_common(1)[0][0]
        except TypeError:
            # answer 是 list/dict 等不可 hash 类型 → 选置信度最高的
            best_idx = max(range(len(answers)), key=lambda i: weights[i])
            return answers[best_idx]

    @staticmethod
    def _aggregate_confidence(opinions: list[ExpertOpinion]) -> float:
        if not opinions:
            return 0.0
        # L3 concerns 会拉低整体信心
        base = sum(o.confidence for o in opinions) / len(opinions)
        penalty = 0.05 * sum(len(o.concerns) for o in opinions if o.layer == "L3")
        return max(0.0, min(1.0, base - penalty))

    @staticmethod
    def _brief(o: ExpertOpinion) -> dict[str, Any]:
        return {
            "role": o.role,
            "layer": o.layer,
            "answer": o.answer,
            "reason": o.reason,
            "confidence": o.confidence,
        }
