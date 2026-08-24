#!/usr/bin/env python3
"""
GeoSDG LocalEmbedder — 本地文本向量化封装。

Default model: BAAI/bge-small-zh-v1.5 (384 dims, ~130MB).
Graceful fallback: deterministic hash-based embedding when sentence-transformers unavailable.

Design principles:
    - Singleton: shared across MemoryStore and KBRetriever
    - Lazy load: model loaded on first embed() call
    - Offline: no external API dependency
    - Fallback-friendly: hash-based pseudo-embedding when libs missing

Usage:
    from agent.shared.embedder import get_embedder
    emb = get_embedder()
    vectors = emb.embed(["土地退化", "SDG 15.3.1"])  # → (2, 384) float32
    q_vec = emb.embed_query("FoM计算方法")            # → (384,) float32
"""

import hashlib
import logging
import os
from typing import List, Optional

import numpy as np

logger = logging.getLogger(__name__)

# ──────────────────────────────────────────────────────────────────────────────
# Constants
# ──────────────────────────────────────────────────────────────────────────────
DEFAULT_MODEL = "BAAI/bge-small-zh-v1.5"
DEFAULT_DIM = 384
# BGE 系列推荐的 query 指令前缀
_QUERY_INSTRUCTION = "为这个句子生成表示以用于检索相关文章："

# ──────────────────────────────────────────────────────────────────────────────
# Fallback embedder — deterministic hash-based pseudo-embedding
# ──────────────────────────────────────────────────────────────────────────────
class _HashEmbedder:
    """Deterministic hash-based embedding when sentence-transformers is unavailable."""

    def __init__(self, dim: int = DEFAULT_DIM):
        self._dim = dim

    def _hash_embed(self, text: str) -> np.ndarray:
        """Generate a deterministic normalized vector from text hash."""
        h = int(hashlib.md5(text.encode("utf-8")).hexdigest(), 16)
        rng = np.random.RandomState(h % (2**32))
        v = rng.randn(self._dim).astype(np.float32)
        v /= np.linalg.norm(v) + 1e-12
        return v

    def embed(self, texts: List[str]) -> np.ndarray:
        if isinstance(texts, str):
            texts = [texts]
        vecs = np.zeros((len(texts), self._dim), dtype=np.float32)
        for i, t in enumerate(texts):
            vecs[i] = self._hash_embed(t)
        return vecs

    def embed_query(self, query: str) -> np.ndarray:
        return self._hash_embed(query)

    @property
    def dim(self) -> int:
        return self._dim

    @property
    def is_fallback(self) -> bool:
        return True


# ──────────────────────────────────────────────────────────────────────────────
# LocalEmbedder — primary class
# ──────────────────────────────────────────────────────────────────────────────
class LocalEmbedder:
    """
    Local text embedding using sentence-transformers.

    Default model: BAAI/bge-small-zh-v1.5 (384 dims, ~130MB).
    Falls back to hash-based embedding when sentence-transformers is unavailable.
    """

    def __init__(self, model_name: str = DEFAULT_MODEL):
        self._model_name = model_name
        self._model = None
        self._dim = DEFAULT_DIM
        self._is_fallback = False
        self._loaded = False
        self._fallback_embedder: Optional[_HashEmbedder] = None

    def _ensure_loaded(self):
        """Lazy-load the model on first use."""
        if self._loaded:
            return
        self._loaded = True
        try:
            from sentence_transformers import SentenceTransformer
            logger.info(f"Loading embedding model: {self._model_name}")
            self._model = SentenceTransformer(self._model_name)
            # Determine actual dimension from model
            test_vec = self._model.encode(["test"])
            self._dim = test_vec.shape[1]
            logger.info(f"Embedding model loaded: dim={self._dim}")
        except ImportError:
            logger.warning(
                "sentence-transformers not installed. "
                "Install with: pip install sentence-transformers"
            )
            self._is_fallback = True
            self._fallback_embedder = _HashEmbedder(self._dim)
        except Exception as e:
            logger.warning(f"Failed to load model {self._model_name}: {e}")
            self._is_fallback = True
            self._fallback_embedder = _HashEmbedder(self._dim)

    def embed(self, texts: List[str]) -> np.ndarray:
        """
        Batch embed text list → (N, dim) float32 array.

        Args:
            texts: List of text strings to embed.

        Returns:
            np.ndarray of shape (N, dim), float32, L2-normalized.
        """
        self._ensure_loaded()
        if isinstance(texts, str):
            texts = [texts]
        if len(texts) == 0:
            return np.zeros((0, self._dim), dtype=np.float32)

        if self._is_fallback:
            return self._fallback_embedder.embed(texts)

        vecs = self._model.encode(texts, normalize_embeddings=True, show_progress_bar=False)
        return np.array(vecs, dtype=np.float32)

    def embed_query(self, query: str) -> np.ndarray:
        """
        Single query embedding with BGE query instruction prefix.

        BGE models recommend adding a query instruction for retrieval tasks.
        This method applies the instruction automatically.

        Args:
            query: Query text string.

        Returns:
            np.ndarray of shape (dim,), float32, L2-normalized.
        """
        self._ensure_loaded()
        if not query or not query.strip():
            return np.zeros(self._dim, dtype=np.float32)

        if self._is_fallback:
            return self._fallback_embedder.embed_query(query)

        # BGE query instruction prefix
        prefixed = _QUERY_INSTRUCTION + query
        vec = self._model.encode([prefixed], normalize_embeddings=True, show_progress_bar=False)
        return np.array(vec[0], dtype=np.float32)

    @property
    def dim(self) -> int:
        """Return embedding dimension."""
        self._ensure_loaded()
        return self._dim

    @property
    def is_fallback(self) -> bool:
        """True if using hash-based fallback (sentence-transformers unavailable)."""
        self._ensure_loaded()
        return self._is_fallback

    @property
    def model_name(self) -> str:
        return self._model_name


# ──────────────────────────────────────────────────────────────────────────────
# Singleton
# ──────────────────────────────────────────────────────────────────────────────
_embedder: Optional[LocalEmbedder] = None


def get_embedder(model_name: str = DEFAULT_MODEL) -> LocalEmbedder:
    """
    Get or create the singleton LocalEmbedder instance.

    Shared across MemoryStore and KBRetriever to avoid loading
    the model multiple times.
    """
    global _embedder
    if _embedder is None:
        _embedder = LocalEmbedder(model_name=model_name)
    return _embedder


# ──────────────────────────────────────────────────────────────────────────────
# CLI test entry
# ──────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    emb = get_embedder()
    print(f"Model: {emb.model_name}, dim={emb.dim}, fallback={emb.is_fallback}")

    texts = ["土地退化指标 SDG 15.3.1", "FoM 精度评估方法", "北京区域参数推荐"]
    vecs = emb.embed(texts)
    print(f"Batch embed: shape={vecs.shape}, dtype={vecs.dtype}")

    q_vec = emb.embed_query("FoM系数计算方法")
    print(f"Query embed: shape={q_vec.shape}")

    # Cosine similarity (vectors are L2-normalized, so dot product = cosine)
    sims = vecs @ q_vec
    for text, sim in zip(texts, sims):
        print(f"  sim={sim:.4f}  {text}")
