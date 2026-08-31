"""GeoSDG-Agent 决策质量打分器.

评的不是"答案对错"这么简单，而是决策会诊的三个质量维度：

    accuracy   决策命中：answer 是否覆盖 gold 关键结论（分区/布尔/关键词）
    evidence   证据覆盖：verdict.evidence 是否引用了 gold 要求的分区/指标，
               且置信度是否达到下限（决策必须"有据可依"）
    consistency 结论一致：同一任务多次会诊，answer 是否稳定（决策不能飘）

每个维度打 0~1 分，总分为三者加权平均。
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Any

# 三维权重（证据覆盖权重最高——决策 Copilot 的核心是"有据可依"）
WEIGHTS = {"accuracy": 0.4, "evidence": 0.4, "consistency": 0.2}


@dataclass
class DimensionScore:
    name: str
    score: float
    detail: str


@dataclass
class CaseScore:
    case_id: str
    dimensions: list[DimensionScore] = field(default_factory=list)

    @property
    def total(self) -> float:
        return round(
            sum(WEIGHTS.get(d.name, 0.0) * d.score for d in self.dimensions), 4
        )

    def as_dict(self) -> dict[str, Any]:
        return {
            "case_id": self.case_id,
            "total": self.total,
            "dimensions": {d.name: {"score": round(d.score, 4), "detail": d.detail} for d in self.dimensions},
        }


# ============================================================================
# 各维度打分函数
# ============================================================================

def score_accuracy(verdict: dict[str, Any], gold: dict[str, Any]) -> DimensionScore:
    """决策命中：answer 覆盖 gold.answer_keywords 的比例."""
    keywords = gold.get("answer_keywords") or []
    if not keywords:
        # 无关键词约束（如点估计）→ 只要有非空 answer 即算命中
        ok = bool(str(verdict.get("answer", "")).strip())
        return DimensionScore("accuracy", 1.0 if ok else 0.0,
                              "no keyword gold; non-empty answer" if ok else "empty answer")
    answer_text = json.dumps(verdict.get("answer", ""), ensure_ascii=False).lower()
    hit = [k for k in keywords if str(k).lower() in answer_text]
    score = len(hit) / len(keywords)
    return DimensionScore("accuracy", score, f"hit {len(hit)}/{len(keywords)}: {hit}")


def score_evidence(verdict: dict[str, Any], gold: dict[str, Any]) -> DimensionScore:
    """证据覆盖：evidence 是否引用 gold 要求的分区/指标 + 置信度达标."""
    evidence = verdict.get("evidence") or []
    cited_parts = {str(e.get("partition")) for e in evidence if isinstance(e, dict)}
    cited_inds = {str(e.get("indicator")) for e in evidence if isinstance(e, dict)}

    need_parts = set(gold.get("must_cite_partitions") or [])
    need_inds = set(gold.get("must_cite_indicators") or [])
    min_conf = float(gold.get("min_confidence") or 0.0)
    conf = float(verdict.get("confidence") or 0.0)

    checks: list[float] = []
    details: list[str] = []

    if need_parts:
        cov = len(need_parts & cited_parts) / len(need_parts)
        checks.append(cov)
        details.append(f"partitions {cov:.0%}")
    if need_inds:
        cov = len(need_inds & cited_inds) / len(need_inds)
        checks.append(cov)
        details.append(f"indicators {cov:.0%}")
    # 置信度达标（1/0）
    conf_ok = 1.0 if conf >= min_conf else 0.0
    checks.append(conf_ok)
    details.append(f"conf {conf:.2f}>={min_conf:.2f}:{'Y' if conf_ok else 'N'}")

    score = sum(checks) / len(checks) if checks else 0.0
    return DimensionScore("evidence", score, "; ".join(details))


def score_consistency(answers: list[Any]) -> DimensionScore:
    """结论一致：多次会诊 answer 的众数占比."""
    if len(answers) <= 1:
        return DimensionScore("consistency", 1.0, "single run (n<=1)")
    norm = [json.dumps(a, ensure_ascii=False, sort_keys=True, default=str) for a in answers]
    top = max(set(norm), key=norm.count)
    score = norm.count(top) / len(norm)
    return DimensionScore("consistency", score, f"mode {norm.count(top)}/{len(norm)}")


def score_case(
    case: dict[str, Any],
    verdicts: list[dict[str, Any]],
) -> CaseScore:
    """对单个 case 的一批 verdict（多次运行）打分.

    Args:
        case: 评测样例（含 gold）.
        verdicts: 同一 case 多次运行返回的 verdict dict 列表（>=1）.

    Returns:
        CaseScore.
    """
    gold = case.get("gold", {})
    primary = verdicts[0]
    cs = CaseScore(case_id=case.get("id", "unknown"))
    cs.dimensions.append(score_accuracy(primary, gold))
    cs.dimensions.append(score_evidence(primary, gold))
    cs.dimensions.append(score_consistency([v.get("answer") for v in verdicts]))
    return cs
