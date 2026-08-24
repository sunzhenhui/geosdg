#!/usr/bin/env python3
"""
kb_rebuild.py — 全量重建知识库索引。

Scans all Markdown files under agent/knowledge/raw/ and rebuilds
the FAISS index + metadata from scratch.

Usage:
    python scripts/kb_rebuild.py
    python scripts/kb_rebuild.py --quiet
"""

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
    import argparse

    parser = argparse.ArgumentParser(
        description="Full rebuild of the GeoSDG knowledge base index",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
This script will:
  1. Clear the existing FAISS index and metadata
  2. Scan all *.md files under agent/knowledge/raw/
  3. Chunk and embed each document
  4. Save the new index and metadata

Use this when:
  - The index is corrupted
  - You want to switch embedding models
  - You've bulk-added many files and want a clean rebuild
        """,
    )

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

    print("🔄 Rebuilding knowledge base index...")
    start = time.time()

    kb = KBRetriever()
    total = kb.rebuild()

    elapsed = time.time() - start

    if not args.quiet:
        stats = kb.stats()
        print(f"\n✅ Knowledge base rebuilt successfully")
        print(f"   Total chunks: {stats['total_chunks']}")
        print(f"   Embedding dim: {stats['embedding_dim']}")
        print(f"   FAISS index: {'yes' if stats['is_faiss'] else 'no (numpy fallback)'}")
        print(f"   Fallback embedder: {'yes' if stats['is_fallback_embedder'] else 'no'}")
        print(f"   Total sources: {stats['total_sources']}")
        print(f"   Total tokens: {stats['total_tokens']}")
        print(f"   Time: {elapsed:.1f}s")
        if stats['sources']:
            print(f"\n   Sources:")
            for src, count in sorted(stats['sources'].items(), key=lambda x: -x[1]):
                print(f"     {count:4d} chunks  {src}")


if __name__ == "__main__":
    main()
