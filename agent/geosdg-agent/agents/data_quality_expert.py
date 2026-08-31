"""L2-领域知识：数据质量专家（元数据层，关键的"质检员"）."""

from __future__ import annotations

from typing import Any

from .base import ExpertBase


class DataQualityExpert(ExpertBase):
    role = "data_quality"
    layer = "L2"
    system_prompt = (
        "You are a geospatial data QA specialist. Assess partition-level data "
        "reliability: coverage gaps, temporal misalignment, nodata ratio, "
        "resolution mismatch. Do NOT answer the substantive question; only "
        "certify whether the underlying data is trustworthy enough."
    )

    def get_knowledge(self, sdg_meta: dict[str, Any]) -> dict[str, Any]:
        """按 meta 快速估算数据质量（骨架版返回 mock 评级）."""
        partitions = sdg_meta.get("partitions", {})
        return {
            "partition_count": len(partitions),
            "resolution": sdg_meta.get("information", {}).get("resolution", "unknown"),
            "coverage_ratio": 0.94,
            "temporal_alignment": "single-year",
            "quality_grade": "B",
        }
