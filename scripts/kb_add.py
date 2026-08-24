#!/usr/bin/env python3
"""
kb_add.py — 向知识库添加知识条目。

Usage:
    # 添加 Markdown 文件
    python scripts/kb_add.py --file path/to/doc.md

    # 添加内联文本
    python scripts/kb_add.py --text "SDG 15.3.1 土地退化中性指标..." --source "manual/sdg15"

    # 添加论文摘要
    python scripts/kb_add.py --file paper.md --type paper --tags "SDG 15,land degradation"

    # 添加评估案例
    python scripts/kb_add.py --file case.md --type case --region "杭州" --year 2024

After adding, the index is automatically saved.
"""

import argparse
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
        description="Add knowledge to the GeoSDG knowledge base",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Add a Markdown file
  python scripts/kb_add.py --file agent/knowledge/raw/cli_usage/sdg-cli.md

  # Add inline text
  python scripts/kb_add.py --text "FoM = Hits / (Hits + Misses + False Alarms)" --source "manual/fom"

  # Add a paper with tags
  python scripts/kb_add.py --file paper.md --type paper --tags "SDG 15,land degradation,2024"

  # Add a case study
  python scripts/kb_add.py --file case.md --type case --region "杭州" --year 2024
        """,
    )

    # Input source (mutually exclusive)
    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument(
        "--file", "-f",
        help="Path to Markdown file to add",
    )
    input_group.add_argument(
        "--text", "-t",
        help="Inline text content to add",
    )

    # Metadata options
    parser.add_argument(
        "--source", "-s",
        help="Source label (default: file path or 'inline')",
    )
    parser.add_argument(
        "--type",
        choices=["indicator", "methodology", "paper", "case", "faq", "general"],
        default="general",
        help="Knowledge type (default: general)",
    )
    parser.add_argument(
        "--tags",
        help="Comma-separated tags (e.g. 'SDG 15,land degradation')",
    )
    parser.add_argument(
        "--region",
        help="Region name (for case studies)",
    )
    parser.add_argument(
        "--year",
        type=int,
        help="Year (for case studies)",
    )

    # Output options
    parser.add_argument(
        "--quiet", "-q",
        action="store_true",
        help="Suppress info output",
    )

    args = parser.parse_args()

    # Setup logging
    level = logging.WARNING if args.quiet else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%H:%M:%S",
    )

    # Initialize KB
    kb = KBRetriever()
    kb.load()

    # Build metadata
    metadata = {"type": args.type}
    if args.tags:
        metadata["tags"] = [t.strip() for t in args.tags.split(",")]
    if args.region:
        metadata["region"] = args.region
    if args.year:
        metadata["year"] = args.year

    # Add content
    start = time.time()
    if args.file:
        if not os.path.isfile(args.file):
            print(f"Error: File not found: {args.file}", file=sys.stderr)
            sys.exit(1)
        source = args.source or args.file
        count = kb.add_document(args.file, source=source)
        # Attach metadata to newly added chunks
        if metadata:
            for i in range(len(kb._metadata) - count, len(kb._metadata)):
                kb._metadata[i].setdefault("metadata", {}).update(metadata)
    else:
        source = args.source or "inline"
        count = kb.add_text(args.text, source=source, metadata=metadata)

    elapsed = (time.time() - start) * 1000

    if count == 0:
        print("Warning: No chunks were added (file may be empty or too small)", file=sys.stderr)
        sys.exit(1)

    # Save
    kb.save()

    # Output
    if not args.quiet:
        print(f"✅ Added {count} chunks to knowledge base")
        print(f"   Source: {source}")
        print(f"   Type: {args.type}")
        if args.tags:
            print(f"   Tags: {args.tags}")
        print(f"   Time: {elapsed:.0f}ms")
        print(f"   Total chunks in KB: {kb.size}")


if __name__ == "__main__":
    main()
