"""决策任务类型 → prompt 模板.

GeoSDG-Agent 不是"做题"，而是为一个研究区做**空间可持续发展决策会诊**：
读现状、看未来、权衡目标间冲突，给出有证据链的决策建议。

因此这里分的不是"题型"（选择题/问答题），而是**决策任务的产物形态**：
- point_estimate    单值诊断（某分区某指标现状值 / UN 达标计数）
- boolean_judgment  是否判定（是否存在政策冲突 / 是否达标）
- ranking           排序建议（优先干预分区排序）
- advisory_report   会诊报告（带证据链 + 置信度 + 反方意见的决策建议）
"""

from __future__ import annotations

from enum import Enum
from string import Template
from typing import Any

from . import common

# ============================================================================
# system prompt
# ============================================================================

system_prompt = (
    "You are a spatial sustainable-development decision copilot. Given a study "
    "region's partitioned SDG indicators, UN thresholds, policy red-lines, "
    "climate scenarios and land-use simulation credibility, you produce "
    "evidence-based decision advice for planners — not exam answers. Always "
    "ground every recommendation in concrete partition-level evidence."
)

# ============================================================================
# 分区类型（SDG 表现单元）
# ============================================================================

partition_kinds = (
    "urban",            # 城镇分区
    "rural",            # 乡村分区
    "cropland",         # 农田分区
    "forest",           # 林地分区
    "water",            # 水域分区
    "grassland",        # 草地分区
    "wetland",          # 湿地分区
    "others",
)

# ============================================================================
# 决策任务产物形态
# ============================================================================

class task_type(Enum):
    point_estimate = 1
    boolean_judgment = 2
    ranking = 3
    advisory_report = 4


task_ability2type: dict[str, task_type] = {
    # diagnosing（现状诊断）
    "diagnosing-indicator_value":       task_type.point_estimate,
    "diagnosing-worst_partition":       task_type.point_estimate,
    "diagnosing-un_status_count":       task_type.point_estimate,

    # locating（分区定位）
    "locating-partition_by_status":     task_type.ranking,
    "locating-partition_by_indicator":  task_type.ranking,

    # comparing（分区间对比）
    "comparing-partition_comparison":   task_type.boolean_judgment,

    # reasoning（跨维度推理）
    "reasoning-longitudinal_trend":     task_type.advisory_report,
    "reasoning-scenario_forecast":      task_type.advisory_report,
    "reasoning-tradeoff_detection":     task_type.advisory_report,

    # analyzing（决策会诊）
    "analyzing-priority_area":          task_type.ranking,
    "analyzing-intervention_plan":      task_type.advisory_report,
    "analyzing-policy_conflict":        task_type.boolean_judgment,
}

# ============================================================================
# 各产物形态的输出契约（决策口吻，非应试口吻）
# ============================================================================

_POINT_CN = (
    '决策诊断任务：基于研究区 $partition_count 个分区的实测数据与知识库，'
    '给出该项现状的确切数值/结论，仅返回 JSON：'
    '{"answer": "...", "reason": "支撑该数值的分区与指标"}'
)
_POINT_EN = (
    'Decision-diagnosis task: from the $partition_count partitions and the '
    'knowledge base, report the exact current value/finding. JSON only: '
    '{"answer": "...", "reason": "which partition & indicator supports it"}'
)

_BOOL_CN = (
    '决策判定任务：基于研究区 $partition_count 个分区数据 + 政策红线 + UN 阈值，'
    '判定是否成立，仅返回 JSON：'
    '{"answer": true, "reason": "触发判定的证据", "confidence": 0.0-1.0}'
)
_BOOL_EN = (
    'Decision-judgment task: from the $partition_count partitions + policy '
    'red-lines + UN thresholds, judge whether it holds. JSON only: '
    '{"answer": true, "reason": "evidence that triggers the verdict", "confidence": 0.0-1.0}'
)

_RANK_CN = (
    '决策排序任务：基于研究区 $partition_count 个分区数据 + UN 阈值 + 政策知识，'
    '给出优先级排序及理由，仅返回 JSON：'
    '{"answer": ["A1", "A3"], "reason": "1. XXX; 2. XXX", '
    '"evidence": [{"partition":"A1","indicator":"SDG_11_3_1","value":2.31,"un_status":"critical"}], '
    '"confidence": 0.85}'
)
_RANK_EN = (
    'Decision-ranking task: from the $partition_count partitions + UN thresholds '
    '+ policy knowledge, rank by priority with rationale. JSON only: '
    '{"answer": ["A1", "A3"], "reason": "1. XXX; 2. XXX", '
    '"evidence": [{"partition":"A1","indicator":"SDG_11_3_1","value":2.31,"un_status":"critical"}], '
    '"confidence": 0.85}'
)

_REPORT_CN = (
    '决策会诊任务：基于研究区 $partition_count 个分区数据 + UN 阈值 + 政策红线 '
    '+ 气候情景 + 模拟可信度，给出带证据链的决策建议，仅返回 JSON：'
    '{"answer": "决策建议正文", "reason": "1. XXX; 2. XXX", '
    '"evidence": [{"partition":"A1","indicator":"SDG_11_3_1","value":2.31,"un_status":"critical"}], '
    '"confidence": 0.85, "concerns": ["反方/不确定性提示"]}'
)
_REPORT_EN = (
    'Decision-advisory task: from the $partition_count partitions + UN thresholds '
    '+ policy red-lines + climate scenarios + simulation credibility, give an '
    'evidence-backed recommendation. JSON only: '
    '{"answer": "recommendation text", "reason": "1. XXX; 2. XXX", '
    '"evidence": [{"partition":"A1","indicator":"SDG_11_3_1","value":2.31,"un_status":"critical"}], '
    '"confidence": 0.85, "concerns": ["counter-arguments / uncertainties"]}'
)


def _pick(cn: str, en: str) -> str:
    return cn if common.language == "cn" else en


point_estimate_format = _pick(_POINT_CN, _POINT_EN)
boolean_judgment_format = _pick(_BOOL_CN, _BOOL_EN)
ranking_format = _pick(_RANK_CN, _RANK_EN)
advisory_report_format = _pick(_REPORT_CN, _REPORT_EN)

# ============================================================================
# 决策任务 → 输出契约模板
# ============================================================================

task_ability2format: dict[str, str] = {
    # diagnosing
    "diagnosing-indicator_value":       point_estimate_format,
    "diagnosing-worst_partition":       point_estimate_format,
    "diagnosing-un_status_count":       point_estimate_format,

    # locating
    "locating-partition_by_status":     ranking_format,
    "locating-partition_by_indicator":  ranking_format,

    # comparing
    "comparing-partition_comparison":   boolean_judgment_format,

    # reasoning
    "reasoning-longitudinal_trend":     advisory_report_format,
    "reasoning-scenario_forecast":      advisory_report_format,
    "reasoning-tradeoff_detection":     advisory_report_format,

    # analyzing
    "analyzing-priority_area":          ranking_format,
    "analyzing-intervention_plan":      advisory_report_format,
    "analyzing-policy_conflict":        boolean_judgment_format,
}


def ability2instruction(ability: str, partition_count: int) -> str:
    """按决策任务生成输出契约模板.

    Args:
        ability: 形如 "analyzing-priority_area" 的决策任务 key.
        partition_count: 参与本次会诊的分区数.

    Returns:
        已完成变量替换的 prompt 文本.
    """
    tmpl = task_ability2format.get(ability, advisory_report_format)
    return Template(tmpl).safe_substitute(partition_count=partition_count)


def get_final_answer(answer: dict[str, Any], ability: str) -> Any:
    """从合议结果里抽取"决策结论"字段."""
    _ = ability
    if isinstance(answer, dict) and "answer" in answer:
        return answer["answer"]
    return answer
