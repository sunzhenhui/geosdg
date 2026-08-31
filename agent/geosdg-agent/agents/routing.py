"""task_type → 专家召唤路由表.

关键设计点：不是所有专家每个决策任务都到场；按任务类型精准召唤，
节省 token 且避免噪声（读一个现状值只需 3 位，分区优先级排序要全班过一遍）.
"""

from __future__ import annotations

# 三层专家的完整清单（用于 fallback / 全召唤模式）
ALL_L1: tuple[str, ...] = ("planner", "ecologist", "economist", "sociologist")
ALL_L2: tuple[str, ...] = ("un", "policy", "climate", "data_quality", "simulation")
ALL_L3: tuple[str, ...] = ("devil_advocate", "statistician")

# 按决策任务的专家召唤矩阵
# key: task_type; value: {"L1": [...], "L2": [...], "L3": [...]}
ROUTING_TABLE: dict[str, dict[str, list[str]]] = {
    # ---------- diagnosing（现状诊断）----------
    "diagnosing-indicator_value": {
        "L1": ["planner"],
        "L2": ["un", "data_quality"],
        "L3": [],
    },
    "diagnosing-worst_partition": {
        "L1": ["planner"],
        "L2": ["un"],
        "L3": ["statistician"],
    },
    "diagnosing-un_status_count": {
        "L1": ["planner"],
        "L2": ["un"],
        "L3": [],
    },

    # ---------- locating（分区定位）----------
    "locating-partition_by_status": {
        "L1": ["planner", "ecologist"],
        "L2": ["un"],
        "L3": [],
    },
    "locating-partition_by_indicator": {
        "L1": ["planner", "ecologist"],
        "L2": ["un"],
        "L3": [],
    },

    # ---------- comparing（分区间对比）----------
    "comparing-partition_comparison": {
        "L1": ["planner"],
        "L2": ["un"],
        "L3": ["statistician"],
    },

    # ---------- reasoning（跨维度推理）----------
    "reasoning-longitudinal_trend": {
        "L1": ["planner"],
        "L2": ["un", "climate"],
        "L3": ["statistician"],
    },
    "reasoning-scenario_forecast": {
        "L1": ["planner", "ecologist"],
        "L2": ["climate", "simulation", "data_quality"],
        "L3": ["devil_advocate"],
    },
    "reasoning-tradeoff_detection": {
        "L1": list(ALL_L1),
        "L2": ["un", "policy"],
        "L3": ["devil_advocate"],
    },

    # ---------- analyzing（决策会诊）----------
    "analyzing-priority_area": {
        "L1": list(ALL_L1),
        "L2": ["un", "policy", "simulation", "data_quality"],
        "L3": ["devil_advocate", "statistician"],
    },
    "analyzing-intervention_plan": {
        "L1": list(ALL_L1),
        "L2": ["policy", "climate", "simulation"],
        "L3": ["devil_advocate"],
    },
    "analyzing-policy_conflict": {
        "L1": ["planner", "ecologist"],
        "L2": ["policy"],
        "L3": ["devil_advocate"],
    },
}


def get_experts_for(task_type: str) -> list[str]:
    """返回参与该决策任务的专家 role 平铺列表；未知任务 → 全 L1 兜底."""
    entry = ROUTING_TABLE.get(task_type)
    if entry is None:
        return list(ALL_L1)
    return entry["L1"] + entry["L2"] + entry["L3"]


def group_by_layer(task_type: str) -> dict[str, list[str]]:
    """按层返回，Moderator 可据此分阶段调度."""
    return ROUTING_TABLE.get(
        task_type,
        {"L1": list(ALL_L1), "L2": [], "L3": []},
    )
