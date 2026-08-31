"""SDG Knowledge Injection (DKI-SDG).

对标 PEACE/modules/DKI.py。
干的事：按分区/指标召回外部知识（政策红线 + UN 阈值 + 气候情景 + 数据质量），
再让 LLM 挑出与问题相关的子集.
"""

from __future__ import annotations

from typing import Any

from ..agents import ClimateExpert, DataQualityExpert, PolicyExpert, UnExpert
from ..utils import api


class SDGKnowledgeInjection:
    """PEACE 里的 `domain_knowledge_injection` 类的 SDG 版本."""

    def __init__(
        self,
        policy: PolicyExpert | None = None,
        un: UnExpert | None = None,
        climate: ClimateExpert | None = None,
        data_quality: DataQualityExpert | None = None,
    ) -> None:
        self.policy = policy or PolicyExpert()
        self.un = un or UnExpert()
        self.climate = climate or ClimateExpert()
        self.data_quality = data_quality or DataQualityExpert()

    def consult(
        self,
        question: str,
        sdg_meta: dict[str, Any] | None,
    ) -> dict[str, Any]:
        """按 sdg_meta 召回知识，并按 question 过滤（对标 dki.consult）."""
        if sdg_meta is None:
            return {}

        region_bbox = sdg_meta.get("region", {}).get("bbox")
        indicators = sdg_meta.get("information", {}).get("indicators", [])

        knowledge: dict[str, Any] = {
            "policy": self.policy.get_knowledge(region_bbox) if region_bbox else {},
            "un": self.un.get_knowledge(indicators) if indicators else {},
            "climate": self.climate.get_knowledge(indicators),
            "data_quality": self.data_quality.get_knowledge(sdg_meta),
        }

        # 让 LLM 挑出真正相关的知识子集（骨架版全量返回）
        return api.select(question, knowledge)
