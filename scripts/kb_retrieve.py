#!/usr/bin/env python3
"""
kb_retrieve.py — 测试知识库语义检索。

Usage:
    python scripts/kb_retrieve.py "土地退化怎么计算"
    python scripts/kb_retrieve.py "FoM计算方法" --top-k 3
    python scripts/kb_retrieve.py "杭州评估" --top-k 5 --source-filter "cases/"
    python scripts/kb_retrieve.py "SDG 11" --json
"""

import argparse
import json
import logging
import os
import sys
import time

# Add project root to path
_PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _PROJECT_ROOT)

from agent.shared.kb_retriever import KBRetriever

logger = logging.getLogger(__name__)


def main():
    parser = argparse.ArgumentParser(
        description="Test semantic retrieval from the GeoSDG knowledge base",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python scripts/kb_retrieve.py "土地退化怎么计算"
  python scripts/kb_retrieve.py "FoM计算方法" --top-k 3
  python scripts/kb_retrieve.py "杭州评估" --source-filter "cases/"
  python scripts/kb_retrieve.py "SDG 11" --json
        """,
    )

    parser.add_argument(
        "query",
        help="Natural language query",
    )
    parser.add_argument(
        "--top-k", "-k",
        type=int,
        default=5,
        help="Number of results (default: 5)",
    )
    parser.add_argument(
        "--source-filter",
        help="Filter results by source prefix",
    )
    parser.add_argument(
        "--min-score",
        type=float,
        default=0.0,
        help="Minimum similarity score (default: 0.0)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output as JSON",
    )
    parser.add_argument(
        "--context",
        action="store_true",
        help="Output formatted for Agent context injection",
    )

    args = parser.parse_args()

    logging.basicConfig(level=logging.WARNING)

    kb = KBRetriever()
    kb.load()

    if kb.size == 0:
        if args.json:
            print(json.dumps({"error": "Knowledge base is empty"}, ensure_ascii=False))
        else:
            print("⚠️  Knowledge base is empty!")
            print("   Run: python scripts/kb_rebuild.py  to build the index")
        sys.exit(1)

    start = time.time()
    results = kb.retrieve(
        query=args.query,
        top_k=args.top_k,
        min_score=args.min_score,
        source_filter=args.source_filter,
    )
    elapsed = (time.time() - start) * 1000

    if args.json:
        output = {
            "query": args.query,
            "top_k": args.top_k,
            "latency_ms": round(elapsed, 1),
            "results": [r.to_dict() for r in results],
        }
        print(json.dumps(output, ensure_ascii=False, indent=2))
        return

    if args.context:
        context = kb.retrieve_for_agent(args.query, top_k=args.top_k)
        print(context)
        return

    # Human-readable output
    print("=" * 70)
    print(f"  Query: \"{args.query}\"")
    print(f"  Results: {len(results)} / Top-{args.top_k}  |  Latency: {elapsed:.0f}ms")
    print("=" * 70)

    if not results:
        print("\n  ❌ No results found.")
        print("  Try rephrasing your query or rebuilding the index.")
        return

    for i, r in enumerate(results, 1):
        print(f"\n  ── Result {i} (score: {r.score:.4f}) ──────────────────")
        if r.breadcrumb:
            print(f"  Section:  {r.breadcrumb}")
        if r.source:
            print(f"  Source:   {r.source}")
        if r.metadata:
            tags = r.metadata.get("tags")
            if tags:
                print(f"  Tags:     {', '.join(tags)}")
            mtype = r.metadata.get("type")
            if mtype:
                print(f"  Type:     {mtype}")
        print(f"  Content:")
        # Indent text
        for line in r.text.split("\n"):
            print(f"    {line}")

    print("\n" + "=" * 70)


if __name__ == "__main__":
    main()
