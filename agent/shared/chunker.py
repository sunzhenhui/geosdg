#!/usr/bin/env python3
"""
GeoSDG MarkdownChunker — Markdown 文档分块器。

Split Markdown knowledge documents into semantically coherent chunks
for embedding and vector storage.

Strategies:
    - heading: split by ## / ### headings (default)
    - fixed: fixed-size sliding window with overlap
    - sentence: sentence-level splitting (for short texts)

Each chunk preserves:
    - source file path
    - heading hierarchy (breadcrumb)
    - chunk index within source
    - token count estimate

Usage:
    from agent.shared.chunker import MarkdownChunker
    chunker = MarkdownChunker(max_tokens=512, overlap_tokens=64)
    chunks = chunker.chunk_file("agent/knowledge/raw/cli_usage/sdg-cli.md")
    chunks = chunker.chunk_text(markdown_text, source="inline")
"""

import logging
import os
import re
from dataclasses import dataclass, field
from typing import List, Optional

logger = logging.getLogger(__name__)

# ──────────────────────────────────────────────────────────────────────────────
# Data classes
# ──────────────────────────────────────────────────────────────────────────────
@dataclass
class Chunk:
    """A single text chunk from a knowledge document."""
    text: str
    source: str = ""
    heading: str = ""
    breadcrumb: str = ""  # e.g. "SDG 15 > 15.3 > 15.3.1"
    chunk_index: int = 0
    token_count: int = 0
    metadata: dict = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "text": self.text,
            "source": self.source,
            "heading": self.heading,
            "breadcrumb": self.breadcrumb,
            "chunk_index": self.chunk_index,
            "token_count": self.token_count,
            "metadata": self.metadata,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "Chunk":
        return cls(
            text=d["text"],
            source=d.get("source", ""),
            heading=d.get("heading", ""),
            breadcrumb=d.get("breadcrumb", ""),
            chunk_index=d.get("chunk_index", 0),
            token_count=d.get("token_count", 0),
            metadata=d.get("metadata", {}),
        )


# ──────────────────────────────────────────────────────────────────────────────
# Token estimation
# ──────────────────────────────────────────────────────────────────────────────
def _estimate_tokens(text: str) -> int:
    """
    Estimate token count for mixed Chinese/English text.

    Rule of thumb:
        - Chinese characters: ~1.5 tokens each
        - English words: ~1 token each
    """
    # Count Chinese characters
    cn_chars = len(re.findall(r'[\u4e00-\u9fff]', text))
    # Remove Chinese, count English words
    en_text = re.sub(r'[\u4e00-\u9fff]', ' ', text)
    en_words = len(en_text.split())
    return int(cn_chars * 1.5 + en_words)


# ──────────────────────────────────────────────────────────────────────────────
# MarkdownChunker
# ──────────────────────────────────────────────────────────────────────────────
class MarkdownChunker:
    """
    Markdown document chunker for knowledge base ingestion.

    Default strategy: heading-based splitting with fallback to
    fixed-size window for oversized sections.
    """

    def __init__(
        self,
        max_tokens: int = 512,
        overlap_tokens: int = 64,
        min_tokens: int = 50,
        strategy: str = "heading",
    ):
        """
        Args:
            max_tokens: Maximum tokens per chunk.
            overlap_tokens: Overlap tokens between chunks (for fixed strategy).
            min_tokens: Minimum tokens per chunk (smaller chunks are merged).
            strategy: Chunking strategy — 'heading', 'fixed', or 'sentence'.
        """
        self._max_tokens = max_tokens
        self._overlap_tokens = overlap_tokens
        self._min_tokens = min_tokens
        self._strategy = strategy

    # ── Public API ──────────────────────────────────────────────────────────

    def chunk_file(self, filepath: str, source: Optional[str] = None) -> List[Chunk]:
        """
        Read a Markdown file and split into chunks.

        Args:
            filepath: Path to the Markdown file.
            source: Override source label (default: filepath).

        Returns:
            List of Chunk objects.
        """
        if not os.path.isfile(filepath):
            logger.warning(f"File not found: {filepath}")
            return []

        with open(filepath, "r", encoding="utf-8") as f:
            text = f.read()

        if not text.strip():
            return []

        src = source or filepath
        return self._chunk(text, src)

    def chunk_text(self, text: str, source: str = "inline") -> List[Chunk]:
        """
        Split a Markdown text string into chunks.

        Args:
            text: Markdown text content.
            source: Source label for metadata.

        Returns:
            List of Chunk objects.
        """
        if not text.strip():
            return []
        return self._chunk(text, source)

    # ── Internal ────────────────────────────────────────────────────────────

    def _chunk(self, text: str, source: str) -> List[Chunk]:
        """Dispatch to strategy-specific chunker."""
        if self._strategy == "heading":
            raw = self._chunk_by_heading(text, source)
        elif self._strategy == "fixed":
            raw = self._chunk_by_fixed(text, source)
        elif self._strategy == "sentence":
            raw = self._chunk_by_sentence(text, source)
        else:
            logger.warning(f"Unknown strategy '{self._strategy}', falling back to 'heading'")
            raw = self._chunk_by_heading(text, source)

        # Post-process: merge tiny chunks, split oversized chunks
        chunks = self._postprocess(raw)
        return chunks

    def _chunk_by_heading(self, text: str, source: str) -> List[Chunk]:
        """
        Split by ## and ### headings.

        Each section (from one heading to the next) becomes a candidate chunk.
        Heading hierarchy is tracked for breadcrumb.
        """
        lines = text.split("\n")
        sections: List[dict] = []
        current = {"heading": "", "breadcrumb": "", "level": 0, "lines": []}
        # Track heading stack for breadcrumb
        heading_stack: List[tuple] = []  # (level, title)

        for line in lines:
            m = re.match(r'^(#{1,6})\s+(.+)', line)
            if m:
                level = len(m.group(1))
                title = m.group(2).strip()

                # Save previous section
                if current["lines"]:
                    sections.append(current.copy())

                # Update heading stack
                # Pop headings at same or deeper level
                while heading_stack and heading_stack[-1][0] >= level:
                    heading_stack.pop()
                heading_stack.append((level, title))

                breadcrumb = " > ".join(h[1] for h in heading_stack)
                current = {
                    "heading": title,
                    "breadcrumb": breadcrumb,
                    "level": level,
                    "lines": [line],  # Include heading line in chunk
                }
            else:
                current["lines"].append(line)

        # Don't forget the last section
        if current["lines"]:
            sections.append(current)

        chunks = []
        for i, sec in enumerate(sections):
            body = "\n".join(sec["lines"]).strip()
            if not body:
                continue
            tokens = _estimate_tokens(body)
            chunks.append(Chunk(
                text=body,
                source=source,
                heading=sec["heading"],
                breadcrumb=sec["breadcrumb"],
                chunk_index=i,
                token_count=tokens,
            ))

        return chunks

    def _chunk_by_fixed(self, text: str, source: str) -> List[Chunk]:
        """Fixed-size sliding window chunking."""
        # Split into sentences/lines for boundary-aware splitting
        segments = re.split(r'(?<=[。！？\n])', text)
        segments = [s for s in segments if s.strip()]

        chunks = []
        current_text = ""
        idx = 0

        for seg in segments:
            candidate = current_text + seg
            if _estimate_tokens(candidate) > self._max_tokens and current_text:
                tokens = _estimate_tokens(current_text)
                chunks.append(Chunk(
                    text=current_text.strip(),
                    source=source,
                    chunk_index=idx,
                    token_count=tokens,
                ))
                idx += 1
                # Overlap: keep last few segments
                overlap_text = self._get_overlap(current_text)
                current_text = overlap_text + seg
            else:
                current_text = candidate

        if current_text.strip():
            tokens = _estimate_tokens(current_text)
            chunks.append(Chunk(
                text=current_text.strip(),
                source=source,
                chunk_index=idx,
                token_count=tokens,
            ))

        return chunks

    def _chunk_by_sentence(self, text: str, source: str) -> List[Chunk]:
        """Sentence-level chunking for short texts."""
        sentences = re.split(r'(?<=[。！？])', text)
        sentences = [s.strip() for s in sentences if s.strip()]

        chunks = []
        current_text = ""
        idx = 0

        for sent in sentences:
            candidate = current_text + sent
            if _estimate_tokens(candidate) > self._max_tokens and current_text:
                tokens = _estimate_tokens(current_text)
                chunks.append(Chunk(
                    text=current_text.strip(),
                    source=source,
                    chunk_index=idx,
                    token_count=tokens,
                ))
                idx += 1
                current_text = sent
            else:
                current_text = candidate

        if current_text.strip():
            tokens = _estimate_tokens(current_text)
            chunks.append(Chunk(
                text=current_text.strip(),
                source=source,
                chunk_index=idx,
                token_count=tokens,
            ))

        return chunks

    def _get_overlap(self, text: str) -> str:
        """Get overlap portion from the end of text."""
        segments = re.split(r'(?<=[。！？\n])', text)
        overlap = ""
        for seg in reversed(segments):
            candidate = seg + overlap
            if _estimate_tokens(candidate) > self._overlap_tokens:
                break
            overlap = candidate
        return overlap

    def _postprocess(self, chunks: List[Chunk]) -> List[Chunk]:
        """
        Post-process chunks:
        1. Split oversized chunks (> max_tokens * 1.5)
        2. Merge tiny chunks (< min_tokens) with previous
        3. Re-index
        """
        result: List[Chunk] = []

        for chunk in chunks:
            # Split oversized
            if chunk.token_count > self._max_tokens * 1.5:
                sub_chunks = self._split_oversized(chunk)
                result.extend(sub_chunks)
            else:
                result.append(chunk)

        # Merge tiny chunks
        merged: List[Chunk] = []
        for chunk in result:
            if (chunk.token_count < self._min_tokens
                    and merged
                    and merged[-1].token_count + chunk.token_count <= self._max_tokens):
                prev = merged[-1]
                prev.text = prev.text + "\n" + chunk.text
                prev.token_count = _estimate_tokens(prev.text)
                # Keep the more specific heading/breadcrumb
                if chunk.breadcrumb and len(chunk.breadcrumb) > len(prev.breadcrumb):
                    prev.heading = chunk.heading
                    prev.breadcrumb = chunk.breadcrumb
            else:
                merged.append(chunk)

        # Re-index
        for i, chunk in enumerate(merged):
            chunk.chunk_index = i

        return merged

    def _split_oversized(self, chunk: Chunk) -> List[Chunk]:
        """Split an oversized chunk using fixed-size strategy."""
        sub = self._chunk_by_fixed(chunk.text, chunk.source)
        # Preserve heading/breadcrumb metadata
        for s in sub:
            s.heading = chunk.heading
            s.breadcrumb = chunk.breadcrumb
            s.metadata.update(chunk.metadata)
        return sub


# ──────────────────────────────────────────────────────────────────────────────
# CLI test entry
# ──────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    sample = """# SDG 15 指标体系

## SDG 15.1 — 森林与陆地生态

### 15.1.1 森林面积占比

**公式**：Forest_ratio = Forest_area / Total_land_area × 100%

**数据要求**：
- LUCC 栅格数据（GeoTIFF）
- 森林类型编码集合（如 {21, 22, 23}）

**归一化**：Min-Max 归一化，参考范围 [0, 100]

### 15.1.2 重要站点保护覆盖率

该指标衡量生物多样性重要站点的保护覆盖情况。

## SDG 15.3 — 土地退化

### 15.3.1 土地退化中性（LDN）

LDN 指标综合了三个子指标：
1. 土地覆盖变化
2. 土地生产力趋势
3. 土地碳储量变化

**计算方法**：采用 One-Out-All-Out (OOAO) 原则，任一子指标退化则整体判定退化。
"""

    chunker = MarkdownChunker(max_tokens=256, min_tokens=30)
    chunks = chunker.chunk_text(sample, source="sdg15_sample.md")

    print(f"Total chunks: {len(chunks)}")
    for c in chunks:
        print(f"\n--- Chunk {c.chunk_index} (tokens≈{c.token_count}) ---")
        print(f"  Heading: {c.heading}")
        print(f"  Breadcrumb: {c.breadcrumb}")
        print(f"  Text preview: {c.text[:80]}...")
