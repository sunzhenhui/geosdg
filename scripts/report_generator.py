#!/usr/bin/env python3
"""
report_generator.py — GeoSDG Report Generator

Renders Markdown reports from Jinja2 templates with data injection.
Supports chart validation and graceful degradation when charts are missing.

Usage:
    python report_generator.py --data <data.json> --template composite --output report.md
    python report_generator.py --data <data.json> --template composite --output report.md --skip-missing-charts
"""

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path

try:
    from jinja2 import Environment, FileSystemLoader, select_autoescape, TemplateNotFound
except ImportError:
    print("Error: jinja2 is required. Install with: pip install jinja2", file=sys.stderr)
    sys.exit(1)


# ============================================================================
# Constants
# ============================================================================

TEMPLATE_DIR = Path(__file__).parent.parent / "agent" / "templates" / "report"
VALID_TEMPLATES = ["composite", "trend", "subregion"]
VALID_OUTPUT_FORMATS = ["markdown", "html", "pdf"]


# ============================================================================
# Chart Validation
# ============================================================================

def validate_charts(data: dict, skip_missing: bool = False) -> dict:
    """
    Validate chart references in data against actual chart files.

    Args:
        data: Report data dictionary containing 'charts' key
        skip_missing: If True, replace missing charts with placeholder info

    Returns:
        Validation result dict with:
        - 'valid': bool
        - 'missing': list of missing chart IDs
        - 'charts': updated charts dict (with placeholders if skip_missing)
    """
    charts = data.get("charts", {})
    if not charts:
        return {"valid": True, "missing": [], "charts": {}}

    missing = []
    updated_charts = {}

    for chart_id, chart_info in charts.items():
        if not chart_info or not isinstance(chart_info, dict):
            if skip_missing:
                updated_charts[chart_id] = {"path": None, "title": chart_id, "missing": True}
                missing.append(chart_id)
            else:
                missing.append(chart_id)
            continue

        chart_path = chart_info.get("path", "")
        if chart_path and os.path.exists(chart_path):
            updated_charts[chart_id] = chart_info
        else:
            if skip_missing:
                updated_charts[chart_id] = {
                    "path": None,
                    "title": chart_info.get("title", chart_id),
                    "missing": True,
                }
                missing.append(chart_id)
            else:
                missing.append(chart_id)

    return {
        "valid": len(missing) == 0,
        "missing": missing,
        "charts": updated_charts,
    }


# ============================================================================
# Report Renderer
# ============================================================================

class ReportGenerator:
    """Renders Markdown reports from Jinja2 templates."""

    def __init__(self, template_dir: Path = None):
        """
        Initialize the report generator.

        Args:
            template_dir: Path to the templates directory. Defaults to agent/templates/report/.
        """
        self.template_dir = template_dir or TEMPLATE_DIR
        if not self.template_dir.exists():
            raise FileNotFoundError(f"Template directory not found: {self.template_dir}")

        self.env = Environment(
            loader=FileSystemLoader(str(self.template_dir)),
            autoescape=select_autoescape(["html", "xml"]),
            trim_blocks=True,
            lstrip_blocks=True,
        )

        # Register global functions
        self.env.globals["now"] = lambda: datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    def render(self, data: dict, template_name: str = "composite",
               skip_missing_charts: bool = False) -> str:
        """
        Render a report from data using the specified template.

        Args:
            data: Report data dictionary
            template_name: Template name (without extension), one of VALID_TEMPLATES
            skip_missing_charts: If True, missing charts are replaced with placeholders

        Returns:
            Rendered Markdown string

        Raises:
            ValueError: If template_name is invalid
            TemplateNotFound: If template file doesn't exist
        """
        if template_name not in VALID_TEMPLATES:
            raise ValueError(
                f"Invalid template '{template_name}'. Valid: {VALID_TEMPLATES}"
            )

        template_file = f"{template_name}.md.j2"
        template = self.env.get_template(template_file)

        # Validate charts
        chart_result = validate_charts(data, skip_missing=skip_missing_charts)
        if not chart_result["valid"] and not skip_missing_charts:
            missing_str = ", ".join(chart_result["missing"])
            raise FileNotFoundError(
                f"Missing chart files for: {missing_str}. "
                "Use --skip-missing-charts to generate report with placeholders."
            )

        # Update data with validated charts
        render_data = dict(data)
        render_data["charts"] = chart_result["charts"]

        # Ensure required fields
        render_data.setdefault("title", "GeoSDG 评估报告")
        render_data.setdefault("period", "—")
        render_data.setdefault("generated_at", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

        # Render
        rendered = template.render(**render_data)
        return rendered

    def render_to_file(self, data: dict, template_name: str = "composite",
                       output_path: str = "report.md",
                       skip_missing_charts: bool = False) -> str:
        """
        Render a report and write to file.

        Args:
            data: Report data dictionary
            template_name: Template name
            output_path: Output file path
            skip_missing_charts: If True, missing charts get placeholders

        Returns:
            Path to the output file
        """
        rendered = self.render(data, template_name, skip_missing_charts)

        output = Path(output_path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8")

        return str(output)


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="GeoSDG Report Generator — Render Markdown reports from Jinja2 templates"
    )
    parser.add_argument(
        "--data", required=True,
        help="Path to JSON data file"
    )
    parser.add_argument(
        "--template", default="composite",
        choices=VALID_TEMPLATES,
        help="Report template (default: composite)"
    )
    parser.add_argument(
        "--output", "-o", default="report.md",
        help="Output file path (default: report.md)"
    )
    parser.add_argument(
        "--skip-missing-charts", action="store_true",
        help="Generate report with placeholders for missing charts"
    )
    parser.add_argument(
        "--template-dir",
        help="Custom template directory (default: agent/templates/report/)"
    )

    args = parser.parse_args()

    # Load data
    data_path = Path(args.data)
    if not data_path.exists():
        print(f"Error: Data file not found: {data_path}", file=sys.stderr)
        sys.exit(1)

    with open(data_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    # Initialize generator
    template_dir = Path(args.template_dir) if args.template_dir else None
    try:
        generator = ReportGenerator(template_dir=template_dir)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    # Render
    try:
        output_path = generator.render_to_file(
            data=data,
            template_name=args.template,
            output_path=args.output,
            skip_missing_charts=args.skip_missing_charts,
        )
        print(f"Report generated: {output_path}")
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: Failed to generate report: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
