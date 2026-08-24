#!/usr/bin/env python3
"""
GeoSDG KBRetriever — 向量知识库检索引擎。

Manages a FAISS index for semantic retrieval of knowledge chunks
and memory records.

Architecture:
    agent/knowledge/
    ├── raw/                    # Source Markdown documents
    │   ├── cli_usage/
    │   ├── regions/
    │   └── cases/
    ├── index/                  # FAISS index + metadata (auto-generated)
    │   ├── kb.index            # FAISS index file
    │   └── kb_meta.json        # Chunk metadata (id, text, source, ...)
    └── README.md

Usage:
    from agent.shared.kb_retriever import KBRetriever
    kb = KBRetriever()
    kb.add_document("agent/knowledge/raw/cli_usage/sdg-cli.md")
    kb.save()
    results = kb.retrieve("FoM计算方法", top_k=5)
"""

import json
import logging
import os
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

from .chunker import Chunk, MarkdownChunker
from .embedder import get_embedder

logger = logging.getLogger(__name__)

# ──────────────────────────────────────────────────────────────────────────────
# Constants
# ──────────────────────────────────────────────────────────────────────────────
# Resolve paths relative to this file
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_AGENT_DIR = os.path.dirname(_THIS_DIR)  # agent/
_KB_DIR = os.path.join(_AGENT_DIR, "knowledge")
_RAW_DIR = os.path.join(_KB_DIR, "raw")
_INDEX_DIR = os.path.join(_KB_DIR, "index")

_INDEX_FILE = os.path.join(_INDEX_DIR, "kb.index")
_META_FILE = os.path.join(_INDEX_DIR, "kb_meta.json")
_VECTORS_FILE = os.path.join(_INDEX_DIR, "kb_vectors.npy")


# ──────────────────────────────────────────────────────────────────────────────
# Data classes
# ──────────────────────────────────────────────────────────────────────────────
@dataclass
class RetrievalResult:
    """A single retrieval result."""
    chunk_id: int
    text: str
    source: str
    heading: str
    breadcrumb: str
    score: float  # cosine similarity [0, 1]
    metadata: dict = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "chunk_id": self.chunk_id,
            "text": self.text,
            "source": self.source,
            "heading": self.heading,
            "breadcrumb": self.breadcrumb,
            "score": round(self.score, 4),
            "metadata": self.metadata,
        }


# ──────────────────────────────────────────────────────────────────────────────
# KBRetriever
# ──────────────────────────────────────────────────────────────────────────────
class KBRetriever:
    """
    Vector knowledge base retriever using FAISS.

    Features:
    - Add documents (Markdown files or raw text)
    - Semantic search via FAISS
    - Persistent storage (index + metadata)
    - Incremental updates (add without full rebuild)
    - Graceful fallback to brute-force numpy search when FAISS unavailable
    """

    def __init__(
        self,
        kb_dir: str = _KB_DIR,
        index_file: str = _INDEX_FILE,
        meta_file: str = _META_FILE,
        vectors_file: str = _VECTORS_FILE,
        max_tokens: int = 512,
        overlap_tokens: int = 64,
    ):
        """
        Args:
            kb_dir: Knowledge base root directory.
            index_file: FAISS index file path.
            meta_file: Metadata JSON file path.
            vectors_file: Numpy vectors file path (fallback when FAISS unavailable).
            max_tokens: Max tokens per chunk.
            overlap_tokens: Overlap tokens for chunking.
        """
        self._kb_dir = kb_dir
        self._index_file = index_file
        self._meta_file = meta_file
        self._vectors_file = vectors_file
        self._raw_dir = os.path.join(kb_dir, "raw")
        self._index_dir = os.path.dirname(index_file)

        # Ensure directories exist
        os.makedirs(self._raw_dir, exist_ok=True)
        os.makedirs(self._index_dir, exist_ok=True)

        # Components
        self._embedder = get_embedder()
        self._chunker = MarkdownChunker(
            max_tokens=max_tokens,
            overlap_tokens=overlap_tokens,
        )

        # State
        self._faiss = None  # faiss.Index instance (lazy import)
        self._vectors: Optional[np.ndarray] = None  # Fallback: numpy array
        self._metadata: List[dict] = []
        self._dim: int = self._embedder.dim
        self._loaded = False

    # ── Properties ──────────────────────────────────────────────────────────

    @property
    def size(self) -> int:
        """Number of chunks in the index."""
        if self._faiss is not None:
            return self._faiss.ntotal
        if self._vectors is not None:
            return self._vectors.shape[0]
        return len(self._metadata)

    @property
    def dim(self) -> int:
        return self._dim

    @property
    def is_faiss(self) -> bool:
        """True if using FAISS, False if using numpy fallback."""
        return self._faiss is not None

    # ── Loading / Saving ────────────────────────────────────────────────────

    def load(self) -> bool:
        """
        Load index and metadata from disk.

        Returns:
            True if loaded successfully, False if not found or error.
        """
        if self._loaded:
            return True
        self._loaded = True

        # Try loading FAISS index
        if os.path.isfile(self._index_file):
            try:
                import faiss
                self._faiss = faiss.read_index(self._index_file)
                self._dim = self._faiss.d
                logger.info(f"FAISS index loaded: {self._faiss.ntotal} vectors, dim={self._dim}")
            except ImportError:
                logger.warning("faiss not available, using numpy fallback for retrieval")
            except Exception as e:
                logger.warning(f"Failed to load FAISS index: {e}")

        # Load metadata
        if os.path.isfile(self._meta_file):
            try:
                with open(self._meta_file, "r", encoding="utf-8") as f:
                    self._metadata = json.load(f)
                logger.info(f"Metadata loaded: {len(self._metadata)} chunks")
            except Exception as e:
                logger.warning(f"Failed to load metadata: {e}")
                self._metadata = []

        # If FAISS not available but metadata exists, try loading numpy vectors
        if self._faiss is None and self._metadata:
            self._load_numpy_vectors()

        return self._faiss is not None or len(self._metadata) > 0

    def _load_numpy_vectors(self):
        """Load numpy vectors from .npy file (fallback when FAISS unavailable)."""
        if self._vectors is not None:
            return  # Already loaded
        if os.path.isfile(self._vectors_file):
            try:
                self._vectors = np.load(self._vectors_file)
                logger.info(f"Numpy vectors loaded: {self._vectors.shape} ← {self._vectors_file}")
            except Exception as e:
                logger.warning(f"Failed to load numpy vectors: {e}")

    def _save_numpy_vectors(self):
        """Save numpy vectors to .npy file (fallback when FAISS unavailable)."""
        if self._vectors is not None and self._vectors.shape[0] > 0:
            try:
                np.save(self._vectors_file, self._vectors)
                logger.info(f"Numpy vectors saved: {self._vectors.shape} → {self._vectors_file}")
            except Exception as e:
                logger.warning(f"Failed to save numpy vectors: {e}")

    def save(self):
        """Save index and metadata to disk."""
        os.makedirs(self._index_dir, exist_ok=True)

        # Save FAISS index
        if self._faiss is not None:
            try:
                import faiss
                faiss.write_index(self._faiss, self._index_file)
                logger.info(f"FAISS index saved: {self._faiss.ntotal} vectors → {self._index_file}")
            except ImportError:
                # FAISS not available — save numpy vectors instead
                self._save_numpy_vectors()
            except Exception as e:
                logger.error(f"Failed to save FAISS index: {e}")
                self._save_numpy_vectors()
        else:
            # No FAISS — save numpy vectors
            self._save_numpy_vectors()

        # Save metadata
        try:
            with open(self._meta_file, "w", encoding="utf-8") as f:
                json.dump(self._metadata, f, ensure_ascii=False, indent=2)
            logger.info(f"Metadata saved: {len(self._metadata)} chunks → {self._meta_file}")
        except Exception as e:
            logger.error(f"Failed to save metadata: {e}")

    # ── Adding documents ────────────────────────────────────────────────────

    def add_document(self, filepath: str, source: Optional[str] = None) -> int:
        """
        Add a Markdown document to the knowledge base.

        Args:
            filepath: Path to the Markdown file.
            source: Override source label.

        Returns:
            Number of chunks added.
        """
        chunks = self._chunker.chunk_file(filepath, source=source)
        if not chunks:
            logger.warning(f"No chunks generated from: {filepath}")
            return 0

        texts = [c.text for c in chunks]
        vecs = self._embedder.embed(texts)
        self._add_vectors(vecs, [c.to_dict() for c in chunks])
        logger.info(f"Added {len(chunks)} chunks from: {filepath}")
        return len(chunks)

    def add_text(self, text: str, source: str = "inline", metadata: Optional[dict] = None) -> int:
        """
        Add raw text to the knowledge base.

        Args:
            text: Text content to add.
            source: Source label.
            metadata: Additional metadata.

        Returns:
            Number of chunks added.
        """
        chunks = self._chunker.chunk_text(text, source=source)
        if not chunks:
            return 0

        # Attach extra metadata
        if metadata:
            for c in chunks:
                c.metadata.update(metadata)

        texts = [c.text for c in chunks]
        vecs = self._embedder.embed(texts)
        self._add_vectors(vecs, [c.to_dict() for c in chunks])
        logger.info(f"Added {len(chunks)} chunks from text (source={source})")
        return len(chunks)

    def add_directory(self, dirpath: str, pattern: str = "*.md") -> int:
        """
        Add all Markdown files from a directory.

        Args:
            dirpath: Directory path.
            pattern: File glob pattern.

        Returns:
            Total number of chunks added.
        """
        import glob
        files = glob.glob(os.path.join(dirpath, "**", pattern), recursive=True)
        total = 0
        for f in sorted(files):
            total += self.add_document(f)
        return total

    def _add_vectors(self, vectors: np.ndarray, metadata_list: List[dict]):
        """Add vectors and metadata to the index."""
        if len(vectors) == 0:
            return

        # Ensure vectors are float32 and contiguous
        vectors = np.ascontiguousarray(vectors, dtype=np.float32)

        # Initialize FAISS index if needed
        if self._faiss is None:
            self._init_faiss(vectors.shape[1])

        # Add to FAISS
        if self._faiss is not None:
            self._faiss.add(vectors)
        else:
            # Numpy fallback
            if self._vectors is None:
                self._vectors = vectors.copy()
            else:
                self._vectors = np.vstack([self._vectors, vectors])

        # Add metadata
        for m in metadata_list:
            m["chunk_id"] = len(self._metadata)
            self._metadata.append(m)

    def _init_faiss(self, dim: int):
        """Initialize FAISS index."""
        self._dim = dim
        try:
            import faiss
            # Use IndexFlatIP for inner product (cosine similarity with normalized vectors)
            self._faiss = faiss.IndexFlatIP(dim)
            logger.info(f"FAISS index initialized: dim={dim}, metric=InnerProduct")
        except ImportError:
            logger.warning("faiss not available, using numpy fallback")
            self._faiss = None

    # ── Retrieval ───────────────────────────────────────────────────────────

    def retrieve(
        self,
        query: str,
        top_k: int = 5,
        min_score: float = 0.0,
        source_filter: Optional[str] = None,
    ) -> List[RetrievalResult]:
        """
        Semantic retrieval of top-K relevant chunks.

        Args:
            query: Natural language query.
            top_k: Number of results to return.
            min_score: Minimum similarity score threshold.
            source_filter: Filter results by source prefix.

        Returns:
            List of RetrievalResult, sorted by score descending.
        """
        if self.size == 0:
            logger.warning("Knowledge base is empty, no results")
            return []

        # Embed query
        q_vec = self._embedder.embed_query(query)  # (dim,)
        q_vec = np.ascontiguousarray(q_vec.reshape(1, -1), dtype=np.float32)

        # Search
        if self._faiss is not None:
            scores, indices = self._faiss.search(q_vec, min(top_k * 3, self.size))
            scores = scores[0]
            indices = indices[0]
        elif self._vectors is not None:
            # Numpy fallback: brute-force cosine similarity
            sims = self._vectors @ q_vec[0]  # (N,)
            top_idx = np.argsort(sims)[::-1][:top_k * 3]
            scores = sims[top_idx]
            indices = top_idx
        else:
            # No vectors loaded — try loading numpy vectors
            self._load_numpy_vectors()
            if self._vectors is not None and self._vectors.shape[0] > 0:
                # Retry with loaded vectors
                sims = self._vectors @ q_vec[0]
                top_idx = np.argsort(sims)[::-1][:top_k * 3]
                scores = sims[top_idx]
                indices = top_idx
            else:
                logger.warning("No vectors available for retrieval. Run kb_rebuild.py first.")
                return []

        # Build results
        results = []
        for score, idx in zip(scores, indices):
            if idx < 0 or idx >= len(self._metadata):
                continue
            if score < min_score:
                continue

            meta = self._metadata[idx]

            # Source filter
            if source_filter and not meta.get("source", "").startswith(source_filter):
                continue

            results.append(RetrievalResult(
                chunk_id=int(idx),
                text=meta.get("text", ""),
                source=meta.get("source", ""),
                heading=meta.get("heading", ""),
                breadcrumb=meta.get("breadcrumb", ""),
                score=float(score),
                metadata=meta.get("metadata", {}),
            ))

            if len(results) >= top_k:
                break

        return results

    def retrieve_for_agent(
        self,
        query: str,
        top_k: int = 5,
    ) -> str:
        """
        Retrieve and format results for Agent context injection.

        Returns a formatted string suitable for system prompt injection.

        Args:
            query: Natural language query.
            top_k: Number of results.

        Returns:
            Formatted context string.
        """
        results = self.retrieve(query, top_k=top_k)
        if not results:
            return ""

        lines = ["[Knowledge Base Retrieval Results]"]
        for i, r in enumerate(results, 1):
            lines.append(f"\n--- Result {i} (score={r.score:.3f}) ---")
            if r.breadcrumb:
                lines.append(f"Section: {r.breadcrumb}")
            if r.source:
                lines.append(f"Source: {r.source}")
            lines.append(f"Content:\n{r.text}")

        return "\n".join(lines)

    # ── Rebuild ─────────────────────────────────────────────────────────────

    def rebuild(self) -> int:
        """
        Full rebuild: clear index and re-ingest all raw documents.

        Returns:
            Total number of chunks indexed.
        """
        logger.info("Starting full knowledge base rebuild...")

        # Clear existing
        self._faiss = None
        self._vectors = None
        self._metadata = []
        self._init_faiss(self._dim)

        # Ingest all raw documents
        total = 0
        if os.path.isdir(self._raw_dir):
            total = self.add_directory(self._raw_dir)

        # Save
        self.save()
        logger.info(f"Rebuild complete: {total} chunks indexed")
        return total

    # ── Stats ───────────────────────────────────────────────────────────────

    def stats(self) -> dict:
        """
        Get knowledge base statistics.

        Returns:
            Dict with stats: total_chunks, dim, is_faiss, sources, etc.
        """
        sources = {}
        for m in self._metadata:
            src = m.get("source", "unknown")
            sources[src] = sources.get(src, 0) + 1

        total_tokens = sum(m.get("token_count", 0) for m in self._metadata)

        return {
            "total_chunks": self.size,
            "embedding_dim": self._dim,
            "is_faiss": self.is_faiss,
            "is_fallback_embedder": self._embedder.is_fallback,
            "total_sources": len(sources),
            "sources": sources,
            "total_tokens": total_tokens,
            "index_file": self._index_file if os.path.isfile(self._index_file) else None,
            "meta_file": self._meta_file if os.path.isfile(self._meta_file) else None,
        }


# ──────────────────────────────────────────────────────────────────────────────
# Singleton
# ──────────────────────────────────────────────────────────────────────────────
_kb: Optional[KBRetriever] = None


def get_kb_retriever() -> KBRetriever:
    """Get or create the singleton KBRetriever instance."""
    global _kb
    if _kb is None:
        _kb = KBRetriever()
        _kb.load()
    return _kb


# ──────────────────────────────────────────────────────────────────────────────
# CLI test entry
# ──────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    kb = KBRetriever()

    # Add sample text
    kb.add_text(
        "# SDG 15.3.1 土地退化中性\n"
        "LDN 指标综合了三个子指标：土地覆盖变化、土地生产力趋势、土地碳储量变化。\n"
        "采用 One-Out-All-Out (OOAO) 原则。\n",
        source="test_sdg15.md",
    )
    kb.add_text(
        "# FoM 精度评估\n"
        "Figure of Merit (FoM) 是 CA 模拟精度评估的核心指标。\n"
        "FoM = Hits / (Hits + Misses + False Alarms)\n",
        source="test_fom.md",
    )
    kb.save()

    print(f"\nKB stats: {kb.stats()}")

    # Retrieve
    results = kb.retrieve("土地退化怎么计算", top_k=3)
    print(f"\nRetrieval results for '土地退化怎么计算':")
    for r in results:
        print(f"  score={r.score:.4f}  source={r.source}")
        print(f"  text: {r.text[:80]}...")

    results = kb.retrieve("FoM计算方法", top_k=3)
    print(f"\nRetrieval results for 'FoM计算方法':")
    for r in results:
        print(f"  score={r.score:.4f}  source={r.source}")
        print(f"  text: {r.text[:80]}...")
