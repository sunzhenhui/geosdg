"""专家抽象基类（新增，供 10 个专家共用）.

设计目标：
- 统一 role / layer / system_prompt / output_schema 四要素
- 统一 answer() 出入参形态，方便 Moderator 编排
- 每个专家用自己的视角回答同一个问题，返回结构化 JSON
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Any, Literal

from ..utils import api, prompt

ExpertLayer = Literal["L1", "L2", "L3"]


@dataclass
class ExpertOpinion:
    """单个专家的意见（Moderator 合议时的原子单位）."""
    role: str                                  # 专家角色标识，如 "planner"
    layer: ExpertLayer                         # L1 / L2 / L3
    answer: Any                                # 该专家给出的答案（string / list / dict 均可）
    reason: str                                # 推理链
    evidence: list[dict[str, Any]] = field(default_factory=list)
    confidence: float = 0.5                    # 0~1
    concerns: list[str] = field(default_factory=list)  # 该专家提出的顾虑
    raw: str = ""                              # LLM 原始返回，便于调试


class ExpertBase:
    """所有专家的基类.

    子类需覆盖：
        role, layer, system_prompt, output_schema, focus_indicators (可选)
    """

    role: str = "base"
    layer: ExpertLayer = "L1"
    system_prompt: str = prompt.system_prompt
    output_schema: str = (
        '{"answer": ..., "reason": "...", "evidence": [...], '
        '"confidence": 0.0-1.0, "concerns": ["..."]}'
    )
    #: 该专家最关注的 SDG 指标（用于筛出 legend 里相关字段，降 token）
    focus_indicators: tuple[str, ...] = ()

    # ------------------------------------------------------------------
    # public
    # ------------------------------------------------------------------

    def answer(
        self,
        question: str,
        question_type: str,
        sdg_meta: dict[str, Any],
        knowledge: dict[str, Any],
    ) -> ExpertOpinion:
        partition_count = len(sdg_meta.get("partitions", {}))
        instruction = prompt.ability2instruction(question_type, partition_count)
        focused_meta = self._focus(sdg_meta)

        messages = [
            {"role": "system", "content": self.system_prompt},
            {
                "role": "user",
                "content": (
                    f"[Your Role]\n{self.role} (layer={self.layer})\n\n"
                    f"[Question]\n{question}\n\n"
                    f"[Instruction]\n{instruction}\n\n"
                    f"[Required Output Schema]\n{self.output_schema}\n\n"
                    f"[Partitions & Indicators]\n{focused_meta}\n\n"
                    f"[Knowledge]\n{knowledge}\n"
                ),
            },
        ]
        raw = api.chat(messages, model=f"mock-{self.role}")
        return self._parse(raw)

    # ------------------------------------------------------------------
    # helpers
    # ------------------------------------------------------------------

    def _focus(self, sdg_meta: dict[str, Any]) -> dict[str, Any]:
        """按 focus_indicators 裁剪 legend，降低 token 与噪声."""
        if not self.focus_indicators:
            return sdg_meta
        legend = sdg_meta.get("legend", {})
        slim_legend: dict[str, dict[str, Any]] = {}
        for pid, inds in legend.items():
            slim_legend[pid] = {k: v for k, v in inds.items() if k in self.focus_indicators}
        return {**sdg_meta, "legend": slim_legend}

    def _parse(self, raw: str) -> ExpertOpinion:
        """把 LLM 原始返回解析为 ExpertOpinion；解析失败退化为纯文本."""
        try:
            obj = json.loads(raw)
            return ExpertOpinion(
                role=self.role,
                layer=self.layer,
                answer=obj.get("answer"),
                reason=obj.get("reason", ""),
                evidence=obj.get("evidence", []) or [],
                confidence=float(obj.get("confidence", 0.5)),
                concerns=obj.get("concerns", []) or [],
                raw=raw,
            )
        except Exception:
            return ExpertOpinion(
                role=self.role, layer=self.layer,
                answer=raw, reason="[unparseable]",
                confidence=0.3, raw=raw,
            )
