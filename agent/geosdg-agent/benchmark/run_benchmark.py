"""GeoSDG-Agent 决策 benchmark 跑批器.

用法：
    python agent/geosdg-agent/benchmark/run_benchmark.py
    python agent/geosdg-agent/benchmark/run_benchmark.py --runs 3   # 每个 case 跑 3 次测一致性

流程：
    加载 cases/*.json → 对每个 case 调 PEDA 会诊拿完整 verdict
    → scorer 三维打分 → 打印明细 + 汇总平均分.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path

# ---- 把带连字符的包目录注册成合法包名 geosdg_agent ----
_BENCH_DIR = Path(__file__).resolve().parent
_PKG_DIR = _BENCH_DIR.parent                       # geosdg-agent/
_AGENT_ROOT = _PKG_DIR.parent                      # agent/
sys.path.insert(0, str(_AGENT_ROOT))
_spec = importlib.util.spec_from_file_location(
    "geosdg_agent", _PKG_DIR / "__init__.py",
    submodule_search_locations=[str(_PKG_DIR)],
)
_mod = importlib.util.module_from_spec(_spec)
sys.modules["geosdg_agent"] = _mod
_spec.loader.exec_module(_mod)

from geosdg_agent.modules import (  # noqa: E402
    HierarchicalPartitionExtraction,
    PartitionExpertDecisionAssembly,
    SDGKnowledgeInjection,
)

# 复用打分器（本地模块）
sys.path.insert(0, str(_BENCH_DIR))
import scorer  # noqa: E402


def load_cases() -> list[dict]:
    cases_dir = _BENCH_DIR / "cases"
    cases = []
    for f in sorted(cases_dir.glob("*.json")):
        cases.append(json.loads(f.read_text(encoding="utf-8")))
    return cases


def run_one(case: dict, hie, dki, peda) -> dict:
    """跑一次 HIE→DKI→PEDA，返回完整 verdict dict."""
    info = hie.digitalize(case["region"])
    knowledge = dki.consult(case["task"], info)
    raw = peda.consult(
        info, knowledge, True, case["region"], case["task"], case["task_type"],
    )
    try:
        return json.loads(raw)
    except Exception:
        return {"answer": raw, "evidence": [], "confidence": 0.0}


def main() -> None:
    ap = argparse.ArgumentParser(description="GeoSDG-Agent decision benchmark")
    ap.add_argument("--runs", type=int, default=1, help="每个 case 运行次数（测一致性）")
    args = ap.parse_args()

    hie = HierarchicalPartitionExtraction()
    dki = SDGKnowledgeInjection()
    peda = PartitionExpertDecisionAssembly()

    cases = load_cases()
    print(f"Loaded {len(cases)} case(s), runs={args.runs}\n" + "=" * 70)

    results = []
    for case in cases:
        verdicts = [run_one(case, hie, dki, peda) for _ in range(args.runs)]
        cs = scorer.score_case(case, verdicts)
        results.append(cs)
        d = cs.as_dict()
        print(f"\n[{d['case_id']}] total={d['total']}")
        for name, v in d["dimensions"].items():
            print(f"    {name:<12} {v['score']:.3f}  ({v['detail']})")

    print("\n" + "=" * 70)
    if results:
        avg = round(sum(r.total for r in results) / len(results), 4)
        by_dim = {}
        for name in scorer.WEIGHTS:
            vals = [d.score for r in results for d in r.dimensions if d.name == name]
            by_dim[name] = round(sum(vals) / len(vals), 4) if vals else 0.0
        print(f"SUMMARY  cases={len(results)}  overall={avg}")
        print("  by dimension:", by_dim)


if __name__ == "__main__":
    main()
