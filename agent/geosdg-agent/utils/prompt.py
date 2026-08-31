"""题型 → prompt 模板（对标 PEACE/utils/prompt.py）.

PEACE 分的是"地图组件"（title/main_map/legend/...），
我们分的是"SDG 分区"（每个地块的 SDG 表现单元）.
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
    "You are an expert in spatial sustainable development assessment. "
    "You analyze partitioned SDG indicators against UN thresholds and policy "
    "constraints, and produce evidence-based intervention recommendations."
)

# ============================================================================
# 分区类型（对标 PEACE 的 components 元组）
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
# 题型分类（对标 PEACE question_type Enum）
# ============================================================================

class question_type(Enum):
    multiple_choice = 1
    true_false = 2
    fill_in_the_blank = 3
    essay = 4


question_ability2type: dict[str, question_type] = {
    # extracting（提取现状）
    "extracting-indicator_value":       question_type.fill_in_the_blank,
    "extracting-worst_partition":       question_type.fill_in_the_blank,
    "extracting-un_status_count":       question_type.fill_in_the_blank,

    # grounding（定位分区）
    "grounding-partition_by_status":    question_type.essay,
    "grounding-partition_by_indicator": question_type.essay,

    # referring（分区间对比）
    "referring-partition_comparison":   question_type.multiple_choice,

    # reasoning（跨维度推理）
    "reasoning-longitudinal_trend":     question_type.essay,
    "reasoning-scenario_forecast":      question_type.essay,
    "reasoning-tradeoff_detection":     question_type.essay,

    # analyzing（决策级会诊）
    "analyzing-priority_area":          question_type.essay,
    "analyzing-intervention_plan":      question_type.essay,
    "analyzing-policy_conflict":        question_type.essay,
}

# ============================================================================
# 通用格式串（对标 PEACE 的 grounding_format / mcq_format / ...）
# ============================================================================

_FILL_CN = (
    '这是一道填空题。基于提供的分区数据（共 $partition_count 个分区）与知识库，'
    '仅返回 JSON：{"answer": "...", "reason": "..."}'
)
_FILL_EN = (
    'This is a fill-in-the-blank question. Based on the $partition_count partitions '
    'and the knowledge base, respond in JSON only: {"answer": "...", "reason": "..."}'
)

_MCQ_CN = (
    '这是一道选择题。基于分区数据（共 $partition_count 个分区）与知识库，'
    '仅返回 JSON：{"answer": "A", "reason": "..."}'
)
_MCQ_EN = (
    'This is a multiple-choice question. Based on the $partition_count partitions '
    'and the knowledge base, respond in JSON only: {"answer": "A", "reason": "..."}'
)

_TF_CN = (
    '这是一道判断题。基于分区数据（共 $partition_count 个分区）与知识库，'
    '仅返回 JSON：{"answer": true, "reason": "..."}'
)
_TF_EN = (
    'This is a true/false question. Based on the $partition_count partitions '
    'and the knowledge base, respond in JSON only: {"answer": true, "reason": "..."}'
)

_ESSAY_CN = (
    '这是一道问答题。基于分区数据（共 $partition_count 个分区）+ UN 阈值 + 政策知识，'
    '给出证据链，仅返回 JSON：'
    '{"answer": ["A1", "A3"], "reason": "1. XXX; 2. XXX", '
    '"evidence": [{"partition":"A1","indicator":"SDG_11_3_1","value":2.31,"un_status":"critical"}], '
    '"confidence": 0.85}'
)
_ESSAY_EN = (
    'This is an essay question. Based on partitions ($partition_count total) + UN thresholds + policy knowledge, '
    'provide an evidence chain and respond in JSON only: '
    '{"answer": ["A1", "A3"], "reason": "1. XXX; 2. XXX", '
    '"evidence": [{"partition":"A1","indicator":"SDG_11_3_1","value":2.31,"un_status":"critical"}], '
    '"confidence": 0.85}'
)


def _pick(cn: str, en: str) -> str:
    return cn if common.language == "cn" else en


fill_in_the_blank_format = _pick(_FILL_CN, _FILL_EN)
mcq_format = _pick(_MCQ_CN, _MCQ_EN)
yes_no_format = _pick(_TF_CN, _TF_EN)
essay_format = _pick(_ESSAY_CN, _ESSAY_EN)

# ============================================================================
# 题型 → 格式模板（对标 PEACE question_ability2format）
# ============================================================================

question_ability2format: dict[str, str] = {
    # extracting
    "extracting-indicator_value":       fill_in_the_blank_format,
    "extracting-worst_partition":       fill_in_the_blank_format,
    "extracting-un_status_count":       fill_in_the_blank_format,

    # grounding
    "grounding-partition_by_status":    essay_format,
    "grounding-partition_by_indicator": essay_format,

    # referring
    "referring-partition_comparison":   mcq_format,

    # reasoning
    "reasoning-longitudinal_trend":     essay_format,
    "reasoning-scenario_forecast":      essay_format,
    "reasoning-tradeoff_detection":     essay_format,

    # analyzing
    "analyzing-priority_area":          essay_format,
    "analyzing-intervention_plan":      essay_format,
    "analyzing-policy_conflict":        essay_format,
}


def ability2instruction(ability: str, partition_count: int) -> str:
    """按题型生成指令模板（对标 PEACE ability2instruction）.

    Args:
        ability: 形如 "analyzing-priority_area" 的题型 key.
        partition_count: 参与本次问答的分区数.

    Returns:
        已完成变量替换的 prompt 文本.
    """
    tmpl = question_ability2format.get(ability, essay_format)
    return Template(tmpl).safe_substitute(partition_count=partition_count)


def get_final_answer(answer: dict[str, Any], ability: str) -> Any:
    """从 LLM 返回的结构里抽取"最终答案"字段（对标 PEACE get_final_answer）."""
    _ = ability
    if isinstance(answer, dict) and "answer" in answer:
        return answer["answer"]
    return answer
