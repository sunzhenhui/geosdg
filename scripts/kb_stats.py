#!/usr/bin/env python3
"""
kb_stats.py — 查看知识库统计信息。

Usage:
    python scripts/kb_stats.py
    python scripts/kb_stats.py --json
"""

import argparse
import json
import logging
import os
import sys

# Add project root to path
_PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _PROJECT_ROOT)

from agent.shared.kb_retriever import KBRetriever

logger = logging.getLogger(__name__)


def main():
    parser = argparse.ArgumentParser(
        description="Show GeoSDG knowledge base statistics",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output as JSON",
    )
    args = parser.parse_args()

    logging.basicConfig(level=logging.WARNING)

    kb = KBRetriever()
    kb.load()
    stats = kb.stats()

    if args.json:
        print(json.dumps(stats, ensure_ascii=False, indent=2))
        return

    # Human-readable output
    print("=" * 60)
    print("  GeoSDG Knowledge Base Statistics")
    print("=" * 60)
    print(f"  Total chunks:       {stats['total_chunks']}")
    print(f"  Embedding dim:      {stats['embedding_dim']}")
    print(f"  FAISS index:        {'✅ yes' if stats['is_faiss'] else '❌ no (numpy fallback)'}")
    print(f"  Fallback embedder:  {'⚠️  yes' if stats['is_fallback_embedder'] else '✅ no (using sentence-transformers)'}")
    print(f"  Total sources:      {stats['total_sources']}")
    print(f"  Total tokens:       {stats['total_tokens']:,}")

    if stats['index_file']:
        import os
        size_mb = os.path.getsize(stats['index_file']) / (1024 * 1024)
        print(f"  Index file:         {stats['index_file']} ({size_mb:.1f} MB)")
    else:
        print(f"  Index file:         (not found)")

    if stats['meta_file']:
        import os
        size_mb = os.path.getsize(stats['meta_file']) / (1024 * 1024)
        print(f"  Metadata file:      {stats['meta_file']} ({size_mb:.1f} MB)")
    else:
        print(f"  Metadata file:      (not found)")

    if stats['sources']:
        print(f"\n  Sources breakdown:")
        print(f"  {'─' * 56}")
        for src, count in sorted(stats['sources'].items(), key=lambda x: -x[1]):
            print(f"  {count:6d} chunks  {src}")

    print("=" * 60)

    if stats['total_chunks'] == 0:
        print("\n  ⚠️  Knowledge base is empty!")
        print("  Run: python scripts/kb_rebuild.py  to build from raw documents")
        print("  Or:  python scripts/kb_add.py --file <path>  to add individual files")


if __name__ == "__main__":
    main()
