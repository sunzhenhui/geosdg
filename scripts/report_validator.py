#!/usr/bin/env python3
"""
report_validator.py — GeoSDG Report Validator

Validates generated reports against 9 quality rules:
1. Required fields completeness
2. Indicator value range reasonability
3. Chart reference validity
4. Chapter structure completeness
5. Data-chart consistency
6. Unit annotation compliance
7. NoData handling annotation
8. Template rendering error check
9. Output file readability

Usage:
    python report_validator.py --report report.md --data data.json
    python report_validator.py --report report.md --data data.json --strict
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional


# ============================================================================
# Validation Result Types
# ============================================================================

class Severity(Enum):
    PASS = "PASS"
    WARN = "WARN"
    FAIL = "FAIL"


@dataclass
class CheckResult:
    """Result of a single validation check."""
    rule_id: int
    rule_name: str
    severity: Severity
    message: str
    details: Optional[str] = None


@dataclass
class ValidationResult:
    """Overall validation result."""
    results: List[CheckResult] = field(default_factory=list)

    @property
    def overall(self) -> Severity:
        """Overall severity based on individual results."""
        if any(r.severity == Severity.FAIL for r in self.results):
            return Severity.FAIL
        if any(r.severity == Severity.WARN for r in self.results):
            return Severity.WARN
        return Severity.PASS

    @property
    def passed(self) -> int:
        return sum(1 for r in self.results if r.severity == Severity.PASS)

    @property
    def warnings(self) -> int:
        return sum(1 for r in self.results if r.severity == Severity.WARN)

    @property
    def failures(self) -> int:
        return sum(1 for r in self.results if r.severity == Severity.FAIL)

    def to_dict(self) -> dict:
        return {
            "overall": self.overall.value,
            "summary": {
                "total": len(self.results),
                "passed": self.passed,
                "warnings": self.warnings,
                "failures": self.failures,
            },
            "checks": [
                {
                    "rule_id": r.rule_id,
                    "rule_name": r.rule_name,
                    "severity": r.severity.value,
                    "message": r.message,
                    "details": r.details,
                }
                for r in self.results
            ],
        }


# ============================================================================
# Validator
# ============================================================================

class ReportValidator:
    """Validates GeoSDG reports against quality rules."""

    REQUIRED_DATA_FIELDS = ["title", "period"]
    REQUIRED_INDICATOR_FIELDS = ["code", "name", "value"]
    INDICATOR_SCORE_RANGE = (0, 100)

    REQUIRED_CHAPTERS = {
        "composite": ["执行摘要", "指标总览", "结论"],
        "trend": ["趋势概述", "时间序列分析", "结论"],
        "subregion": ["区域概述", "区域指标", "建议"],
    }

    def __init__(self, strict: bool = False):
        self.strict = strict

    def validate(self, report_path: str, data: dict,
                 template_name: str = "composite") -> ValidationResult:
        """
        Run all validation checks on a report.

        Args:
            report_path: Path to the generated Markdown report
            data: Data dictionary used to generate the report
            template_name: Template type used

        Returns:
            ValidationResult with all check results
        """
        result = ValidationResult()

        report_content = ""
        report_file = Path(report_path)
        if report_file.exists():
            report_content = report_file.read_text(encoding="utf-8")
        else:
            result.results.append(CheckResult(
                rule_id=9, rule_name="Output file readability",
                severity=Severity.FAIL,
                message=f"Report file not found: {report_path}",
            ))
            return result

        result.results.append(self._check_required_fields(data))
        result.results.append(self._check_indicator_ranges(data))
        result.results.append(self._check_chart_references(data, report_content))
        result.results.append(self._check_chapter_structure(report_content, template_name))
        result.results.append(self._check_data_chart_consistency(data, report_content))
        result.results.append(self._check_unit_annotations(data))
        result.results.append(self._check_nodata_annotation(data, report_content))
        result.results.append(self._check_template_rendering(report_content))
        result.results.append(self._check_file_readability(report_file))

        if self.strict:
            for r in result.results:
                if r.severity == Severity.WARN:
                    r.severity = Severity.FAIL

        return result

    # ========================================================================
    # Rule 1: Required fields completeness
    # ========================================================================

    def _check_required_fields(self, data: dict) -> CheckResult:
        missing = []
        for field_name in self.REQUIRED_DATA_FIELDS:
            if field_name not in data or not data[field_name]:
                missing.append(field_name)

        if missing:
            return CheckResult(
                rule_id=1, rule_name="Required fields completeness",
                severity=Severity.FAIL,
                message=f"Missing required fields: {', '.join(missing)}",
                details=f"Required: {self.REQUIRED_DATA_FIELDS}",
            )

        return CheckResult(
            rule_id=1, rule_name="Required fields completeness",
            severity=Severity.PASS,
            message="All required fields present.",
        )

    # ========================================================================
    # Rule 2: Indicator value range reasonability
    # ========================================================================

    def _check_indicator_ranges(self, data: dict) -> CheckResult:
        indicators = data.get("indicators", [])
        if not indicators:
            return CheckResult(
                rule_id=2, rule_name="Indicator value range",
                severity=Severity.WARN,
                message="No indicators found in data.",
            )

        out_of_range = []
        for ind in indicators:
            value = ind.get("value")
            if value is None:
                continue
            try:
                num_value = float(value)
                if "score" in ind.get("name", "").lower() or \
                   "score" in ind.get("code", "").lower():
                    if not (self.INDICATOR_SCORE_RANGE[0] <= num_value <=
                            self.INDICATOR_SCORE_RANGE[1]):
                        out_of_range.append(f"{ind.get('code', '?')}: {num_value}")
            except (ValueError, TypeError):
                out_of_range.append(f"{ind.get('code', '?')}: non-numeric value '{value}'")

        if out_of_range:
            return CheckResult(
                rule_id=2, rule_name="Indicator value range",
                severity=Severity.WARN,
                message=f"Values outside expected range: {', '.join(out_of_range)}",
            )

        return CheckResult(
            rule_id=2, rule_name="Indicator value range",
            severity=Severity.PASS,
            message=f"All {len(indicators)} indicator values within range.",
        )

    # ========================================================================
    # Rule 3: Chart reference validity
    # ========================================================================

    def _check_chart_references(self, data: dict, report_content: str) -> CheckResult:
        charts = data.get("charts", {})
        if not charts:
            return CheckResult(
                rule_id=3, rule_name="Chart reference validity",
                severity=Severity.PASS,
                message="No charts referenced (acceptable for text-only reports).",
            )

        missing_files = []
        for chart_id, chart_info in charts.items():
            if not chart_info or not isinstance(chart_info, dict):
                continue
            chart_path = chart_info.get("path", "")
            if chart_path and not os.path.exists(chart_path):
                missing_files.append(f"{chart_id}: {chart_path}")

        placeholder_count = report_content.count("[图表占位]")

        if missing_files:
            return CheckResult(
                rule_id=3, rule_name="Chart reference validity",
                severity=Severity.WARN if placeholder_count > 0 else Severity.FAIL,
                message=f"Missing chart files: {', '.join(missing_files)}",
                details=f"Placeholders in report: {placeholder_count}",
            )

        return CheckResult(
            rule_id=3, rule_name="Chart reference validity",
            severity=Severity.PASS,
            message=f"All {len(charts)} chart files exist.",
        )

    # ========================================================================
    # Rule 4: Chapter structure completeness
    # ========================================================================

    def _check_chapter_structure(self, report_content: str, template_name: str) -> CheckResult:
        required = self.REQUIRED_CHAPTERS.get(template_name, [])
        if not required:
            return CheckResult(
                rule_id=4, rule_name="Chapter structure",
                severity=Severity.PASS,
                message="No specific chapter requirements for this template.",
            )

        missing_chapters = [ch for ch in required if ch not in report_content]

        if missing_chapters:
            return CheckResult(
                rule_id=4, rule_name="Chapter structure",
                severity=Severity.FAIL,
                message=f"Missing required chapters: {', '.join(missing_chapters)}",
                details=f"Required for '{template_name}': {required}",
            )

        return CheckResult(
            rule_id=4, rule_name="Chapter structure",
            severity=Severity.PASS,
            message=f"All required chapters present for '{template_name}'.",
        )

    # ========================================================================
    # Rule 5: Data-chart consistency
    # ========================================================================

    def _check_data_chart_consistency(self, data: dict, report_content: str) -> CheckResult:
        charts = data.get("charts", {})
        if not charts:
            return CheckResult(
                rule_id=5, rule_name="Data-chart consistency",
                severity=Severity.PASS,
                message="No charts to verify.",
            )

        not_in_report = []
        for chart_id in charts:
            chart_info = charts[chart_id]
            chart_path = chart_info.get("path", "") if isinstance(chart_info, dict) else ""
            chart_filename = Path(chart_path).name if chart_path else chart_id

            if chart_id not in report_content and chart_filename not in report_content:
                not_in_report.append(chart_id)

        if not_in_report:
            return CheckResult(
                rule_id=5, rule_name="Data-chart consistency",
                severity=Severity.WARN,
                message=f"Charts in data but not in report: {', '.join(not_in_report)}",
            )

        return CheckResult(
            rule_id=5, rule_name="Data-chart consistency",
            severity=Severity.PASS,
            message=f"All charts referenced in report.",
        )

    # ========================================================================
    # Rule 6: Unit annotation compliance
    # ========================================================================

    def _check_unit_annotations(self, data: dict) -> CheckResult:
        indicators = data.get("indicators", [])
        if not indicators:
            return CheckResult(
                rule_id=6, rule_name="Unit annotation",
                severity=Severity.PASS,
                message="No indicators to check.",
            )

        missing_units = [
            ind.get("code", ind.get("name", "?"))
            for ind in indicators
            if "unit" not in ind or not ind["unit"]
        ]

        if missing_units:
            return CheckResult(
                rule_id=6, rule_name="Unit annotation",
                severity=Severity.WARN,
                message=f"Indicators missing unit: {', '.join(missing_units)}",
            )

        return CheckResult(
            rule_id=6, rule_name="Unit annotation",
            severity=Severity.PASS,
            message=f"All {len(indicators)} indicators have units.",
        )

    # ========================================================================
    # Rule 7: NoData handling annotation
    # ========================================================================

    def _check_nodata_annotation(self, data: dict, report_content: str) -> CheckResult:
        has_nodata = any(
            isinstance(ind, dict) and ind.get("nodata_handled")
            for ind in data.get("indicators", [])
        )
        nodata_mentioned = "NoData" in report_content or "nodata" in report_content.lower()

        if has_nodata and not nodata_mentioned:
            return CheckResult(
                rule_id=7, rule_name="NoData handling annotation",
                severity=Severity.WARN,
                message="Data contains NoData indicators but report doesn't mention NoData handling.",
            )

        return CheckResult(
            rule_id=7, rule_name="NoData handling annotation",
            severity=Severity.PASS,
            message="NoData handling properly documented." if has_nodata
            else "No NoData indicators detected.",
        )

    # ========================================================================
    # Rule 8: Template rendering error check
    # ========================================================================

    def _check_template_rendering(self, report_content: str) -> CheckResult:
        error_patterns = [
            r"\{\{.*\}\}",
            r"\{%.*%\}",
            r"UndefinedError",
            r"TemplateNotFound",
            r"TypeError:.*NoneType",
        ]

        errors_found = []
        for pattern in error_patterns:
            matches = re.findall(pattern, report_content)
            if matches:
                errors_found.extend(matches[:3])

        if errors_found:
            return CheckResult(
                rule_id=8, rule_name="Template rendering",
                severity=Severity.FAIL,
                message=f"Template rendering errors detected: {errors_found}",
            )

        return CheckResult(
            rule_id=8, rule_name="Template rendering",
            severity=Severity.PASS,
            message="No template rendering errors detected.",
        )

    # ========================================================================
    # Rule 9: Output file readability
    # ========================================================================

    def _check_file_readability(self, report_file: Path) -> CheckResult:
        if not report_file.exists():
            return CheckResult(
                rule_id=9, rule_name="Output file readability",
                severity=Severity.FAIL,
                message=f"Report file does not exist: {report_file}",
            )

        file_size = report_file.stat().st_size
        if file_size == 0:
            return CheckResult(
                rule_id=9, rule_name="Output file readability",
                severity=Severity.FAIL,
                message="Report file is empty (0 bytes).",
            )

        if file_size < 100:
            return CheckResult(
                rule_id=9, rule_name="Output file readability",
                severity=Severity.WARN,
                message=f"Report file is suspiciously small ({file_size} bytes).",
            )

        try:
            content = report_file.read_text(encoding="utf-8")
            if not content.strip():
                return CheckResult(
                    rule_id=9, rule_name="Output file readability",
                    severity=Severity.FAIL,
                    message="Report file contains only whitespace.",
                )
        except Exception as e:
            return CheckResult(
                rule_id=9, rule_name="Output file readability",
                severity=Severity.FAIL,
                message=f"Failed to read report file: {e}",
            )

        return CheckResult(
            rule_id=9, rule_name="Output file readability",
            severity=Severity.PASS,
            message=f"Report file is readable ({file_size} bytes).",
        )


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="GeoSDG Report Validator — Validate reports against 9 quality rules"
    )
    parser.add_argument(
        "--report", required=True,
        help="Path to the generated Markdown report"
    )
    parser.add_argument(
        "--data", required=True,
        help="Path to JSON data file used to generate the report"
    )
    parser.add_argument(
        "--template", default="composite",
        choices=["composite", "trend", "subregion"],
        help="Report template type (default: composite)"
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="Treat warnings as failures"
    )
    parser.add_argument(
        "--json-output", action="store_true",
        help="Output result as JSON"
    )

    args = parser.parse_args()

    # Load data
    data_path = Path(args.data)
    if not data_path.exists():
        print(f"Error: Data file not found: {data_path}", file=sys.stderr)
        sys.exit(1)

    with open(data_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    # Validate
    validator = ReportValidator(strict=args.strict)
    result = validator.validate(
        report_path=args.report,
        data=data,
        template_name=args.template,
    )

    # Output
    if args.json_output:
        print(json.dumps(result.to_dict(), ensure_ascii=False, indent=2))
    else:
        print(f"\n{'='*60}")
        print(f"GeoSDG Report Validation Result")
        print(f"{'='*60}")
        print(f"Overall: {result.overall.value}")
        print(f"Summary: {result.passed} passed, {result.warnings} warnings, {result.failures} failures")
        print(f"{'-'*60}")
        for r in result.results:
            icon = {"PASS": "✅", "WARN": "⚠️", "FAIL": "❌"}[r.severity.value]
            print(f"  {icon} [{r.rule_id}] {r.rule_name}: {r.message}")
            if r.details:
                print(f"      Details: {r.details}")
        print(f"{'='*60}\n")

    sys.exit(0 if result.overall != Severity.FAIL else 1)


if __name__ == "__main__":
    main()
