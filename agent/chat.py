#!/usr/bin/env python3
"""GeoSDG-Agent 交互式多轮对话测试.

用法:
    # Mock 模式（默认，无需 LLM API Key）
    python agent/chat.py

    # 接入真实 LLM
    python agent/chat.py --api-key sk-xxx --base-url https://api.deepseek.com/v1 --model deepseek-chat

    # 指定研究区数据
    python agent/chat.py --region data/rasters/lucc_demo_2020.tif

支持的决策任务类型:
    diagnosing-indicator_value       现状诊断：单值
    diagnosing-worst_partition       现状诊断：最差分区
    diagnosing-un_status_count       现状诊断：UN 达标计数
    locating-partition_by_status     分区定位：按状态
    locating-partition_by_indicator  分区定位：按指标
    comparing-partition_comparison   分区间对比
    reasoning-longitudinal_trend     跨维度推理：纵向趋势
    reasoning-scenario_forecast      跨维度推理：情景预测
    reasoning-tradeoff_detection     跨维度推理：权衡检测
    analyzing-priority_area          决策会诊：优先区域
    analyzing-intervention_plan      决策会诊：干预方案
    analyzing-policy_conflict        决策会诊：政策冲突
"""

from __future__ import annotations

import argparse
import importlib
import json
import sys
from pathlib import Path
from typing import Any

# 将 agent/ 加入 sys.path，使 geosdg-agent 包可被 import
AGENT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(AGENT_DIR))

# geosdg-agent 目录名含连字符，用 importlib 加载
_pkg = importlib.import_module("geosdg-agent.copilot")
sdg_copilot = _pkg.sdg_copilot

# 也加载 utils 以便配置
_api_mod = importlib.import_module("geosdg-agent.utils.api")
_common_mod = importlib.import_module("geosdg-agent.utils.common")
_prompt_mod = importlib.import_module("geosdg-agent.utils.prompt")

# ============================================================================
# 预设研究区
# ============================================================================

PRESET_REGIONS: dict[str, dict[str, Any]] = {
    "demo": {
        "name": "Demo City",
        "bbox": (113.0, 22.0, 114.5, 23.5),
        "lucc_path": "data/rasters/lucc_demo_2020.tif",
        "resolution": "30m",
        "year": 2020,
    },
    "guangdong": {
        "name": "Guangdong Province",
        "bbox": (109.6, 20.0, 117.3, 25.5),
        "lucc_path": "data/rasters/lucc_demo_2020.tif",
        "resolution": "30m",
        "year": 2020,
    },
}

# ============================================================================
# 任务类型速查
# ============================================================================

TASK_TYPES = list(_prompt_mod.task_ability2type.keys())

TASK_ALIASES: dict[str, str] = {
    "1":  "diagnosing-indicator_value",
    "2":  "diagnosing-worst_partition",
    "3":  "diagnosing-un_status_count",
    "4":  "locating-partition_by_status",
    "5":  "locating-partition_by_indicator",
    "6":  "comparing-partition_comparison",
    "7":  "reasoning-longitudinal_trend",
    "8":  "reasoning-scenario_forecast",
    "9":  "reasoning-tradeoff_detection",
    "10": "analyzing-priority_area",
    "11": "analyzing-intervention_plan",
    "12": "analyzing-policy_conflict",
    # 中文缩写
    "诊断":   "diagnosing-indicator_value",
    "最差":   "diagnosing-worst_partition",
    "达标":   "diagnosing-un_status_count",
    "定位":   "locating-partition_by_status",
    "对比":   "comparing-partition_comparison",
    "趋势":   "reasoning-longitudinal_trend",
    "预测":   "reasoning-scenario_forecast",
    "权衡":   "reasoning-tradeoff_detection",
    "优先":   "analyzing-priority_area",
    "干预":   "analyzing-intervention_plan",
    "冲突":   "analyzing-policy_conflict",
}

# ============================================================================
# 命令处理
# ============================================================================

def resolve_task_type(raw: str) -> str | None:
    """解析用户输入的任务类型（支持编号/别名/全名）."""
    raw = raw.strip()
    if raw in TASK_ALIASES:
        return TASK_ALIASES[raw]
    if raw in TASK_TYPES:
        return raw
    # 模糊匹配
    for t in TASK_TYPES:
        if raw in t:
            return t
    return None


def print_help() -> None:
    """打印帮助信息."""
    print("""
╔══════════════════════════════════════════════════════════════╗
║              GeoSDG-Agent 交互式对话测试                      ║
╠══════════════════════════════════════════════════════════════╣
║  命令:                                                       ║
║    /help              显示此帮助                              ║
║    /tasks             列出所有决策任务类型                     ║
║    /region [name]     切换研究区 (demo/guangdong)             ║
║    /mode [modes]      设置启用阶段 (HIE,DKI,PEDA)            ║
║    /echo [on|off]     开关中间过程输出                        ║
║    /lang [cn|en]      切换语言                                ║
║    /history           查看对话历史                            ║
║    /quit              退出                                    ║
║                                                              ║
║  用法:                                                       ║
║    直接输入决策问题 → 自动识别 task_type 并执行               ║
║    或: <task_type> <问题>  手动指定任务类型                   ║
║    例: analyzing-priority_area 哪些分区应优先干预？           ║
║    例: 优先 哪些分区应优先干预？                              ║
╚══════════════════════════════════════════════════════════════╝
""")


def print_tasks() -> None:
    """列出所有决策任务类型."""
    print("\n📋 决策任务类型列表:")
    print("─" * 70)
    for i, t in enumerate(TASK_TYPES, 1):
        tt = _prompt_mod.task_ability2type[t]
        print(f"  {i:>2}. {t:<42} → {tt.name}")
    print("─" * 70)
    print("提示: 可用编号(1-12)或中文缩写快速选择\n")


def infer_task_type(question: str) -> str:
    """从自然语言问题推断 task_type（简单关键词匹配）."""
    q = question.lower()
    mapping = [
        (["优先", "priority", "干预区", "重点"], "analyzing-priority_area"),
        (["干预方案", "intervention", "措施", "对策"], "analyzing-intervention_plan"),
        (["冲突", "conflict", "矛盾", "政策冲突"], "analyzing-policy_conflict"),
        (["趋势", "trend", "变化趋势", "纵向"], "reasoning-longitudinal_trend"),
        (["预测", "forecast", "情景", "scenario", "未来"], "reasoning-scenario_forecast"),
        (["权衡", "tradeoff", "协同", "冲突检测"], "reasoning-tradeoff_detection"),
        (["对比", "compar", "比较", "差异"], "comparing-partition_comparison"),
        (["最差", "worst", "最严重"], "diagnosing-worst_partition"),
        (["达标", "un_status", "联合国"], "diagnosing-un_status_count"),
        (["指标值", "indicator_value", "现状值", "数值"], "diagnosing-indicator_value"),
        (["定位", "locat", "找出", "哪些分区"], "locating-partition_by_status"),
    ]
    for keywords, task_type in mapping:
        if any(kw in q for kw in keywords):
            return task_type
    # 默认：决策会诊
    return "analyzing-priority_area"


def run_chat(args: argparse.Namespace) -> None:
    """主交互循环."""
    # 配置 LLM Provider
    llm_provider = None
    if args.api_key:
        provider_cls = _api_mod.OpenAICompatibleProvider
        llm_provider = provider_cls(
            api_key=args.api_key,
            base_url=args.base_url,
            model=args.model,
            temperature=args.temperature,
            max_tokens=args.max_tokens,
        )
        print(f"✅ LLM Provider: {args.base_url} / {args.model}")
    else:
        print("ℹ️  Mock 模式（未提供 --api-key，使用 MockProvider）")

    # 初始研究区
    current_region_name = args.region_preset
    region_data = PRESET_REGIONS.get(current_region_name, PRESET_REGIONS["demo"]).copy()
    if args.region and Path(args.region).exists():
        region_data["lucc_path"] = args.region
        print(f"✅ LUCC 数据: {args.region}")

    # 模式
    copilot_modes: list[str] = ["HIE", "DKI", "PEDA"]
    history: list[dict[str, str]] = []

    print(f"✅ 研究区: {region_data['name']}")
    print(f"✅ 启用阶段: {', '.join(copilot_modes)}")
    print(f"✅ 语言: {_common_mod.language}")
    print()
    print_help()

    while True:
        try:
            user_input = input("🧑 ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n👋 再见！")
            break

        if not user_input:
            continue

        # ---- 命令处理 ----
        if user_input.startswith("/"):
            cmd = user_input.lower()

            if cmd in ("/quit", "/exit", "/q"):
                print("👋 再见！")
                break

            elif cmd in ("/help", "/h", "/?"):
                print_help()

            elif cmd in ("/tasks", "/task", "/t"):
                print_tasks()

            elif cmd.startswith("/region"):
                parts = user_input.split(maxsplit=1)
                if len(parts) > 1:
                    name = parts[1].strip()
                    if name in PRESET_REGIONS:
                        current_region_name = name
                        region_data = PRESET_REGIONS[name].copy()
                        print(f"✅ 切换到研究区: {region_data['name']}")
                    else:
                        print(f"❌ 未知研究区 '{name}'，可选: {', '.join(PRESET_REGIONS)}")
                else:
                    print(f"📍 当前研究区: {region_data['name']} ({current_region_name})")

            elif cmd.startswith("/mode"):
                parts = user_input.split(maxsplit=1)
                if len(parts) > 1:
                    modes = [m.strip().upper() for m in parts[1].split(",")]
                    valid = {"HIE", "DKI", "PEDA"}
                    copilot_modes = [m for m in modes if m in valid]
                    if not copilot_modes:
                        copilot_modes = ["HIE", "DKI", "PEDA"]
                    print(f"✅ 启用阶段: {', '.join(copilot_modes)}")
                else:
                    print(f"📍 当前阶段: {', '.join(copilot_modes)}")

            elif cmd.startswith("/echo"):
                parts = user_input.split(maxsplit=1)
                if len(parts) > 1:
                    _common_mod.echo = parts[1].strip().lower() in ("on", "true", "1", "yes")
                print(f"📍 echo = {_common_mod.echo}")

            elif cmd.startswith("/lang"):
                parts = user_input.split(maxsplit=1)
                if len(parts) > 1:
                    lang = parts[1].strip().lower()
                    if lang in ("cn", "en"):
                        _common_mod.language = lang
                        # 重新加载 prompt 模块以应用语言变更
                        importlib.reload(_prompt_mod)
                    else:
                        print("❌ 语言只支持 cn / en")
                print(f"📍 language = {_common_mod.language}")

            elif cmd in ("/history", "/hist"):
                if not history:
                    print("📭 暂无对话历史")
                else:
                    print("\n📜 对话历史:")
                    print("─" * 50)
                    for i, h in enumerate(history, 1):
                        print(f"  [{i}] Q: {h['question'][:60]}")
                        print(f"      A: {h['answer'][:80]}")
                        print()
                    print("─" * 50)

            else:
                print(f"❌ 未知命令: {user_input}  输入 /help 查看帮助")
            continue

        # ---- 决策问题处理 ----
        # 尝试解析 "task_type 问题" 格式
        task_type: str | None = None
        question: str = user_input

        # 检查是否以 task_type 开头
        first_word = user_input.split(maxsplit=1)[0] if " " in user_input else ""
        if first_word:
            resolved = resolve_task_type(first_word)
            if resolved:
                task_type = resolved
                question = user_input.split(maxsplit=1)[1] if " " in user_input else user_input

        # 自动推断 task_type
        if task_type is None:
            task_type = infer_task_type(question)

        print(f"\n🔍 任务类型: {task_type}")
        print(f"📝 决策问题: {question}")
        print(f"🏗️  启用阶段: {', '.join(copilot_modes)}")
        print("─" * 50)

        # 调用 copilot
        try:
            result = sdg_copilot(
                region_data=region_data,
                task=question,
                task_type=task_type,
                copilot_modes=copilot_modes,
                llm_provider=llm_provider,
            )
        except Exception as e:
            print(f"\n❌ 执行出错: {e}")
            import traceback
            traceback.print_exc()
            continue

        # 展示结果
        print("─" * 50)
        if isinstance(result, dict):
            print("\n📊 决策结果:")
            print(json.dumps(result, ensure_ascii=False, indent=2))
        else:
            print(f"\n📊 决策结果: {result}")

        # 记录历史
        history.append({
            "question": question,
            "task_type": task_type,
            "answer": str(result)[:200],
        })
        print()


# ============================================================================
# 入口
# ============================================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description="GeoSDG-Agent 交互式多轮对话测试",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--api-key", default="",
        help="LLM API Key（不提供则走 Mock 模式）",
    )
    parser.add_argument(
        "--base-url", default="https://api.deepseek.com/v1",
        help="OpenAI 兼容端点（默认 DeepSeek）",
    )
    parser.add_argument(
        "--model", default="deepseek-chat",
        help="模型名（默认 deepseek-chat）",
    )
    parser.add_argument(
        "--temperature", type=float, default=0.2,
        help="采样温度（默认 0.2）",
    )
    parser.add_argument(
        "--max-tokens", type=int, default=2048,
        help="单次回复最大 token（默认 2048）",
    )
    parser.add_argument(
        "--region", default="",
        help="LUCC 栅格路径（覆盖预设）",
    )
    parser.add_argument(
        "--region-preset", default="demo",
        choices=list(PRESET_REGIONS.keys()),
        help="预设研究区（默认 demo）",
    )
    args = parser.parse_args()
    run_chat(args)


if __name__ == "__main__":
    main()
