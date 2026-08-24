#!/usr/bin/env python3
"""
GeoSDG Agent MemoryStore — 3-layer pragmatic memory.

    Layer 1: Session   → session.json    (current session auto-save/restore)
    Layer 2: Archive   → archive/*.json  (completed sessions, vector-searchable)
    Layer 3: Registry  → registry.json + preferences.json (data index + user prefs)

Design principles:
    - Auto-save at key lifecycle events (not manual JSON editing)
    - Search-first: every layer supports keyword + semantic retrieval
    - Fallback-friendly: vector search optional, keyword search always works
    - All local, no external API dependency

Usage in Agent workflows:
    mem = MemoryStore()

    # Router
    mem.session_start("assess SDG 15.3.1 for Beijing", intent="sdg-calc")
    related = mem.search("Beijing land degradation")   # → past sessions + data files

    # Planner
    mem.session_update(plan=["sdg1531", "ca-accuracy"], total_steps=2)

    # Executor (per step)
    mem.session_update(current_step=1, active_data_files=["/data/beijing_lucc.tif"])
    mem.register_file("/data/beijing_lucc.tif", region="Beijing", year=2020, category="LUCC")

    # Session end
    mem.session_finish(summary="SDG 15.3.1 score=72.3, CA FoM=0.41", score=72.3)
"""

import json
import os
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# ──────────────────────────────────────────────────────────────────────────────
# Paths — 项目根 memory/ 目录（git-ignored，等同 reports/）
# ──────────────────────────────────────────────────────────────────────────────
_MEMORY_ROOT = Path(__file__).resolve().parent.parent.parent / "memory"
_SESSION_FILE = _MEMORY_ROOT / "session.json"
_ARCHIVE_DIR = _MEMORY_ROOT / "archive"
_REGISTRY_FILE = _MEMORY_ROOT / "registry.json"
_PREFERENCES_FILE = _MEMORY_ROOT / "preferences.json"

# ──────────────────────────────────────────────────────────────────────────────
# Optional vector embedder — lazy, graceful fallback
# ──────────────────────────────────────────────────────────────────────────────
_embedder = None
_EMBED_DIM = 384


def _get_embedder():
    global _embedder
    if _embedder is not None:
        return _embedder
    try:
        from sentence_transformers import SentenceTransformer
        _embedder = SentenceTransformer("BAAI/bge-small-zh-v1.5")
    except Exception:
        _embedder = _FallbackEmbedder(_EMBED_DIM)
    return _embedder


def _get_faiss():
    try:
        import faiss
        return faiss
    except ImportError:
        return None


class _FallbackEmbedder:
    """Deterministic hash-based embedding when sentence-transformers is unavailable."""
    def __init__(self, dim=384):
        self._dim = dim

    def encode(self, texts, **kwargs):
        import numpy as np
        if isinstance(texts, str):
            texts = [texts]
        vecs = np.zeros((len(texts), self._dim), dtype=np.float32)
        for i, t in enumerate(texts):
            h = abs(hash(t)) % 100000
            rng = np.random.RandomState(h)
            v = rng.randn(self._dim).astype(np.float32)
            v /= np.linalg.norm(v) + 1e-12
            vecs[i] = v
        return vecs


# ──────────────────────────────────────────────────────────────────────────────
# Utility: token-based keyword scoring (always available)
# ──────────────────────────────────────────────────────────────────────────────
def _keyword_score(query: str, text: str) -> float:
    """Simple token-overlap score. Returns 0.0 .. 1.0."""
    if not query or not text:
        return 0.0
    q_tokens = set(re.findall(r'[\w\u4e00-\u9fff]+', query.lower()))
    t_tokens = set(re.findall(r'[\w\u4e00-\u9fff]+', text.lower()))
    if not q_tokens:
        return 0.0
    return len(q_tokens & t_tokens) / len(q_tokens)


# ──────────────────────────────────────────────────────────────────────────────
# MemoryStore
# ──────────────────────────────────────────────────────────────────────────────
class MemoryStore:
    """
    GeoSDG Agent MemoryStore — 3-layer pragmatic memory.

    Layer 1: Session   — session.json (current session state, auto load/save)
    Layer 2: Archive   — archive/*.json (past sessions, searchable)
    Layer 3: Registry  — registry.json + preferences.json
    """

    def __init__(self, root: Path = _MEMORY_ROOT):
        self._root = root
        self._root.mkdir(parents=True, exist_ok=True)
        (_SESSION_FILE.parent / "archive").resolve().mkdir(parents=True, exist_ok=True)
        self._session: Dict[str, Any] = self._load_json(_SESSION_FILE) or self._empty_session()

    # ── JSON I/O helpers ─────────────────────────────────────────────────
    @staticmethod
    def _load_json(path: Path) -> Optional[dict]:
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (FileNotFoundError, json.JSONDecodeError):
            return None

    @staticmethod
    def _save_json(path: Path, data: dict):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")

    def _now_iso(self) -> str:
        return datetime.now(timezone.utc).isoformat()

    def _session_id(self) -> str:
        return datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")

    # ══════════════════════════════════════════════════════════════════════
    # Layer 1 — Session State
    # ══════════════════════════════════════════════════════════════════════

    @staticmethod
    def _empty_session() -> dict:
        return {
            "session_id": None,
            "created_at": None,
            "updated_at": None,
            "status": "idle",
            "intent": None,
            "query": None,
            "plan": [],
            "current_step": 0,
            "total_steps": 0,
            "active_data_files": [],
            "result": None,
            "summary": None,
        }

    def session_start(self, query: str, intent: Optional[str] = None) -> str:
        """Start new session. Auto-saves to session.json. Returns session_id."""
        sid = self._session_id()
        now = self._now_iso()
        self._session = self._empty_session()
        self._session.update(session_id=sid, created_at=now, updated_at=now,
                             status="running", intent=intent, query=query)
        self._save_json(_SESSION_FILE, self._session)
        return sid

    def session_update(self, **kwargs):
        """Update session fields. Auto-saves."""
        self._session.update(kwargs)
        self._session["updated_at"] = self._now_iso()
        self._save_json(_SESSION_FILE, self._session)

    def session_snapshot(self) -> dict:
        """Return a shallow copy of current session."""
        return dict(self._session)

    def session_finish(self, summary: str, result: Any = None):
        """
        Finish current session: save summary, archive, reset.
        """
        self._session.update(
            status="completed",
            summary=summary,
            result=result,
            updated_at=self._now_iso(),
        )
        self._session["plan"] = [s for s in self._session.get("plan", []) if s]
        self._archive_session(dict(self._session))
        self._save_json(_SESSION_FILE, self._empty_session())

    def session_active(self) -> bool:
        """Is a session currently running?"""
        return self._session.get("status") == "running"

    # ══════════════════════════════════════════════════════════════════════
    # Layer 2 — Archive (searchable session history)
    # ══════════════════════════════════════════════════════════════════════

    _ARCHIVE_INDEX_FILE = _MEMORY_ROOT / "archive_index.json"

    def _archive_session(self, session: dict):
        """Write session to archive + update vector index (if embedder available)."""
        sid = session.get("session_id")
        if not sid:
            return
        archive_path = _ARCHIVE_DIR / f"{sid}.json"
        self._save_json(archive_path, session)

        # Build search text
        search_text = " ".join(filter(None, [
            session.get("query", ""),
            session.get("intent", ""),
            session.get("summary", ""),
        ]))
        vector = self._embed_text(search_text)

        # Update index
        index_data = self._load_json(self._ARCHIVE_INDEX_FILE) or {"entries": [], "vectors": []}
        index_data.setdefault("entries", [])
        index_data.setdefault("vectors", [])
        index_data["entries"].append({
            "session_id": sid,
            "created_at": session.get("created_at"),
            "intent": session.get("intent"),
            "query": session.get("query"),
            "summary": session.get("summary"),
            "result": session.get("result"),
        })
        if vector is not None:
            index_data["vectors"].append({
                "session_id": sid,
                "vector": vector.tolist() if hasattr(vector, "tolist") else list(vector),
            })
        self._save_json(self._ARCHIVE_INDEX_FILE, index_data)

    def _embed_text(self, text: str):
        """Embed text → numpy array (384,), or None if embedding unavailable."""
        if not text or not text.strip():
            return None
        try:
            emb = _get_embedder()
            return emb.encode(text)
        except Exception:
            return None

    def search(self, query: str, top_k: int = 5) -> List[Dict]:
        """
        Search archived sessions + registered files by semantic/keyword relevance.

        Returns list of {type, session_id|file_path, score, summary, ...}.
        """
        results = []

        # 1. Archive keyword search (always works)
        index_data = self._load_json(self._ARCHIVE_INDEX_FILE)
        if index_data:
            entries = index_data.get("entries", [])
            for entry in entries:
                score = _keyword_score(query, json.dumps(entry, ensure_ascii=False))
                if score > 0:
                    results.append({
                        "type": "session",
                        "session_id": entry.get("session_id"),
                        "score": round(score, 3),
                        "query": entry.get("query"),
                        "summary": entry.get("summary"),
                        "result": entry.get("result"),
                        "created_at": entry.get("created_at"),
                    })

        # 2. Archive vector search (if embedder + FAISS available)
        faiss = _get_faiss()
        if faiss and index_data and index_data.get("vectors"):
            import numpy as np
            q_vec = self._embed_text(query)
            if q_vec is not None:
                entries = index_data.get("entries", [])
                vec_entries = index_data.get("vectors", [])
                vec_map = {v["session_id"]: v["vector"] for v in vec_entries}

                # Build & search FAISS index
                vectors = np.array([vec_map[e["session_id"]] for e in entries
                                    if e["session_id"] in vec_map], dtype=np.float32)
                if len(vectors) > 0:
                    idx = faiss.IndexFlatIP(_EMBED_DIM)
                    idx.add(vectors)
                    q = np.array([q_vec], dtype=np.float32)
                    D, I = idx.search(q, min(top_k, len(vectors)))
                    existing_ids = {r.get("session_id") for r in results}
                    for dist, i in zip(D[0], I[0]):
                        if i >= 0 and i < len(entries):
                            e = entries[i]
                            sid = e.get("session_id")
                            if sid not in existing_ids:
                                results.append({
                                    "type": "session",
                                    "session_id": sid,
                                    "score": round(float(dist), 3),
                                    "query": e.get("query"),
                                    "summary": e.get("summary"),
                                    "result": e.get("result"),
                                    "created_at": e.get("created_at"),
                                    "_source": "vector",
                                })

        # 3. Data registry search
        reg = self.registry_list()
        for f in reg:
            score = _keyword_score(query, json.dumps(f, ensure_ascii=False))
            if score > 0:
                results.append({
                    "type": "file",
                    "path": f.get("path"),
                    "score": round(score, 3),
                    "region": f.get("region"),
                    "year": f.get("year"),
                    "category": f.get("category"),
                    "alias": f.get("alias"),
                })

        results.sort(key=lambda r: r.get("score", 0), reverse=True)
        return results[:top_k]

    def list_sessions(self, limit: int = 20) -> List[Dict]:
        """List recent sessions from archive index, newest first."""
        index_data = self._load_json(self._ARCHIVE_INDEX_FILE)
        if not index_data:
            return []
        entries = index_data.get("entries", [])
        entries.sort(key=lambda e: e.get("created_at", ""), reverse=True)
        return entries[:limit]

    def get_session(self, session_id: str) -> Optional[dict]:
        """Load full archived session by ID."""
        path = _ARCHIVE_DIR / f"{session_id}.json"
        return self._load_json(path)

    # ══════════════════════════════════════════════════════════════════════
    # Layer 3 — Data Registry
    # ══════════════════════════════════════════════════════════════════════

    def register_file(self, path: str, region: str, year: int,
                      category: Optional[str] = None, alias: Optional[str] = None):
        """Register a data file in registry.json."""
        reg = self._load_json(_REGISTRY_FILE) or {"files": [], "last_updated": None}
        files = reg.setdefault("files", [])

        entry = {
            "path": str(path),
            "region": region,
            "year": year,
            "category": category or "unknown",
            "alias": alias or f"{region}_{category or 'data'}_{year}",
            "first_used": self._now_iso(),
            "last_used": self._now_iso(),
            "used_count": 1,
            "exists": os.path.exists(str(path)),
        }

        # Update existing or add new
        for i, f in enumerate(files):
            if f.get("path") == str(path):
                f2 = dict(files[i])
                f2.update(last_used=self._now_iso(), exists=entry["exists"],
                          used_count=f2.get("used_count", 0) + 1)
                if region and not f2.get("region"):
                    f2["region"] = region
                if year and not f2.get("year"):
                    f2["year"] = year
                files[i] = f2
                reg["last_updated"] = self._now_iso()
                self._save_json(_REGISTRY_FILE, reg)
                return

        files.append(entry)
        reg["last_updated"] = self._now_iso()
        self._save_json(_REGISTRY_FILE, reg)

    def registry_list(self, region: Optional[str] = None,
                      category: Optional[str] = None,
                      year: Optional[int] = None) -> List[Dict]:
        """List registered files with optional filters."""
        reg = self._load_json(_REGISTRY_FILE)
        if not reg:
            return []
        files = reg.get("files", [])
        if region:
            files = [f for f in files if f.get("region") == region]
        if category:
            files = [f for f in files if f.get("category") == category]
        if year:
            files = [f for f in files if f.get("year") == year]
        return files

    def detect_reusable(self, region: str) -> List[Dict]:
        """Return registered files matching a region."""
        return self.registry_list(region=region)

    # ══════════════════════════════════════════════════════════════════════
    # Report Registry
    # ══════════════════════════════════════════════════════════════════════

    _REPORTS_FILE = _MEMORY_ROOT / "reports_index.json"

    def register_report(self, report_path: str, region: str,
                        report_type: str, period: str,
                        template: Optional[str] = None,
                        formats: Optional[List[str]] = None,
                        chart_count: int = 0,
                        validation_result: Optional[dict] = None) -> str:
        """
        Register a generated report in reports_index.json.

        Args:
            report_path: Path to the report directory
            region: Assessment region name
            report_type: Report type (composite/trend/subregion)
            period: Report period (e.g. '2020-2025')
            template: Template name used
            formats: Output formats generated
            chart_count: Number of charts in the report
            validation_result: Validation result dict (from report_validator)

        Returns:
            Report ID string
        """
        report_id = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        reg = self._load_json(self._REPORTS_FILE) or {"reports": [], "last_updated": None}
        reports = reg.setdefault("reports", [])

        entry = {
            "report_id": report_id,
            "path": str(report_path),
            "region": region,
            "report_type": report_type,
            "period": period,
            "template": template or report_type,
            "formats": formats or ["markdown"],
            "chart_count": chart_count,
            "validation": validation_result,
            "created_at": self._now_iso(),
            "exists": os.path.exists(str(report_path)),
        }

        reports.append(entry)
        reg["last_updated"] = self._now_iso()
        self._save_json(self._REPORTS_FILE, reg)
        return report_id

    def list_reports(self, region: Optional[str] = None,
                     report_type: Optional[str] = None,
                     period: Optional[str] = None,
                     limit: int = 20) -> List[Dict]:
        """
        List registered reports with optional filters.

        Args:
            region: Filter by region name
            report_type: Filter by report type (composite/trend/subregion)
            period: Filter by period
            limit: Maximum number of results

        Returns:
            List of report entry dicts, newest first
        """
        reg = self._load_json(self._REPORTS_FILE)
        if not reg:
            return []
        reports = reg.get("reports", [])
        if region:
            reports = [r for r in reports if r.get("region") == region]
        if report_type:
            reports = [r for r in reports if r.get("report_type") == report_type]
        if period:
            reports = [r for r in reports if r.get("period") == period]
        reports.sort(key=lambda r: r.get("created_at", ""), reverse=True)
        return reports[:limit]

    def registry_cleanup_stale(self) -> int:
        """Remove entries for files that no longer exist. Returns count removed."""
        reg = self._load_json(_REGISTRY_FILE)
        if not reg:
            return 0
        files = reg.get("files", [])
        keep = [f for f in files if os.path.exists(f.get("path", ""))]
        removed = len(files) - len(keep)
        reg["files"] = keep
        reg["last_updated"] = self._now_iso()
        self._save_json(_REGISTRY_FILE, reg)
        return removed

    # ══════════════════════════════════════════════════════════════════════
    # User Preferences
    # ══════════════════════════════════════════════════════════════════════

    def preferences(self) -> dict:
        """Read user preferences (always returns a valid dict)."""
        return self._load_json(_PREFERENCES_FILE) or {
            "user_id": "default",
            "default_crs": "EPSG:4326",
            "focus_regions": [],
            "preferred_indicators": [],
            "default_params": {},
            "language": "zh-CN",
            "report_detail_level": "standard",
            "log_path": None,
            "updated_at": None,
        }

    def update_preferences(self, **kwargs):
        """Merge preferences and save."""
        prefs = self.preferences()
        prefs.update(kwargs)
        prefs["updated_at"] = self._now_iso()
        self._save_json(_PREFERENCES_FILE, prefs)


# ──────────────────────────────────────────────────────────────────────────────
# Singleton convenience
# ──────────────────────────────────────────────────────────────────────────────
_MEMORY_STORE: Optional[MemoryStore] = None


def get_memory() -> MemoryStore:
    """Get or create the singleton MemoryStore instance."""
    global _MEMORY_STORE
    if _MEMORY_STORE is None:
        _MEMORY_STORE = MemoryStore()
    return _MEMORY_STORE


# ──────────────────────────────────────────────────────────────────────────────
# CLI test entry
# ──────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    mem = MemoryStore()

    # Demo: start → update → finish
    sid = mem.session_start("评估北京市 SDG 15.3.1 土地退化指标", intent="sdg-calc")
    print(f"Session started: {sid}")

    mem.session_update(plan=["sdg1531_compute", "ca_accuracy"], total_steps=2)
    mem.session_update(current_step=1, active_data_files=["/data/beijing/lucc_2020.tif"])

    mem.register_file("/data/beijing/lucc_2020.tif", region="Beijing", year=2020, category="LUCC")

    mem.session_finish(
        summary="SDG 15.3.1 score=68.5, CA FoM=0.38, 土地退化面积 12.3%",
        result={"score": 68.5, "fom": 0.38, "degraded_pct": 12.3},
    )
    print("Session archived.")

    # Search
    results = mem.search("北京土地退化")
    for r in results:
        print(f"  [{r['type']}] score={r['score']:.3f}  {r.get('summary', '')[:60]}")

    # List
    sessions = mem.list_sessions()
    print(f"\nTotal archived sessions: {len(sessions)}")

    # Registry
    files = mem.registry_list(region="Beijing")
    print(f"Beijing files: {len(files)}")

    # Preferences
    prefs = mem.preferences()
    print(f"Preferred indicators: {prefs.get('preferred_indicators')}")
