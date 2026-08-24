#!/usr/bin/env python3
"""
report_exporter.py — GeoSDG Report Exporter

Exports Markdown reports to HTML and PDF formats.
- HTML: Markdown → HTML via markdown library, with embedded CSS and base64 images
- PDF: HTML → PDF via WeasyPrint, with graceful degradation

Usage:
    python report_exporter.py --input report.md --output report.html --format html
    python report_exporter.py --input report.md --output report.pdf --format pdf
    python report_exporter.py --input report.md --output-dir ./output --formats html,pdf
"""

import argparse
import base64
import json
import mimetypes
import os
import re
import sys
from pathlib import Path

try:
    import markdown
    from markdown.extensions.tables import TableExtension
    from markdown.extensions.fenced_code import FencedCodeExtension
except ImportError:
    print("Error: markdown is required. Install with: pip install markdown", file=sys.stderr)
    sys.exit(1)

try:
    from jinja2 import Environment, FileSystemLoader
except ImportError:
    print("Error: jinja2 is required. Install with: pip install jinja2", file=sys.stderr)
    sys.exit(1)


# ============================================================================
# Constants
# ============================================================================

TEMPLATE_DIR = Path(__file__).parent.parent / "agent" / "templates" / "report"
DEFAULT_CSS_PATH = TEMPLATE_DIR / "styles.css"
COVER_TEMPLATE = "cover.html.j2"


# ============================================================================
# Image Embedding
# ============================================================================

def embed_image_base64(image_path: str) -> str:
    """
    Embed an image file as base64 data URI.

    Args:
        image_path: Path to the image file

    Returns:
        Base64 data URI string, or original path if file not found
    """
    path = Path(image_path)
    if not path.exists():
        # Return a placeholder for missing images
        return f"data:image/svg+xml;base64,{_placeholder_svg_base64()}"

    mime_type, _ = mimetypes.guess_type(str(path))
    if not mime_type:
        mime_type = "image/png"

    with open(path, "rb") as f:
        encoded = base64.b64encode(f.read()).decode("utf-8")

    return f"data:{mime_type};base64,{encoded}"


def _placeholder_svg_base64() -> str:
    """Generate a base64-encoded SVG placeholder for missing images."""
    svg = (
        '<svg xmlns="http://www.w3.org/2000/svg" width="400" height="200" '
        'viewBox="0 0 400 200">'
        '<rect width="400" height="200" fill="#f8f9fa" stroke="#e0e0e0"/>'
        '<text x="200" y="100" text-anchor="middle" fill="#999" '
        'font-size="16" font-family="sans-serif">[图表占位]</text>'
        '</svg>'
    )
    return base64.b64encode(svg.encode("utf-8")).decode("utf-8")


def embed_images_in_html(html_content: str, base_dir: str) -> str:
    """
    Replace image src references with base64 data URIs.

    Args:
        html_content: HTML string with <img> tags
        base_dir: Base directory for resolving relative image paths

    Returns:
        HTML string with embedded images
    """
    base = Path(base_dir)

    def replace_src(match):
        src = match.group(1)
        # Skip already-embedded images
        if src.startswith("data:"):
            return match.group(0)

        # Resolve relative path
        img_path = base / src
        data_uri = embed_image_base64(str(img_path))
        return f'src="{data_uri}"'

    return re.sub(r'src="([^"]+)"', replace_src, html_content)


# ============================================================================
# HTML Export
# ============================================================================

def markdown_to_html(md_content: str, base_dir: str = ".",
                     embed_images: bool = True) -> str:
    """
    Convert Markdown content to styled HTML.

    Args:
        md_content: Markdown string
        base_dir: Base directory for resolving image paths
        embed_images: If True, embed images as base64

    Returns:
        HTML string (body content only, no <html> wrapper)
    """
    extensions = [
        TableExtension(),
        FencedCodeExtension(),
        "toc",
        "attr_list",
    ]

    html_body = markdown.markdown(
        md_content,
        extensions=extensions,
        output_format="html5",
    )

    if embed_images:
        html_body = embed_images_in_html(html_body, base_dir)

    return html_body


def export_html(md_content: str, output_path: str,
                title: str = "GeoSDG Report",
                period: str = "",
                subtitle: str = "",
                embed_images: bool = True,
                base_dir: str = ".") -> str:
    """
    Export Markdown report as a standalone HTML file.

    Args:
        md_content: Markdown string
        output_path: Output HTML file path
        title: Report title
        period: Report period
        subtitle: Report subtitle
        embed_images: If True, embed images as base64
        base_dir: Base directory for resolving image paths

    Returns:
        Path to the output HTML file
    """
    # Convert Markdown to HTML body
    body_html = markdown_to_html(md_content, base_dir, embed_images)

    # Load CSS
    css_content = ""
    if DEFAULT_CSS_PATH.exists():
        css_content = DEFAULT_CSS_PATH.read_text(encoding="utf-8")

    # Use cover template for full HTML
    try:
        env = Environment(loader=FileSystemLoader(str(TEMPLATE_DIR)))
        template = env.get_template(COVER_TEMPLATE)
        full_html = template.render(
            title=title,
            subtitle=subtitle,
            period=period,
            generated_at=_now_str(),
            css=css_content,
            body_html=body_html,
        )
    except Exception:
        # Fallback: simple HTML wrapper if template fails
        full_html = _fallback_html(title, period, css_content, body_html)

    # Write output
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(full_html, encoding="utf-8")

    return str(output)


def _fallback_html(title: str, period: str, css: str, body_html: str) -> str:
    """Generate a simple HTML wrapper when Jinja2 template is unavailable."""
    return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>{title}</title>
    <style>{css}</style>
</head>
<body>
    <h1>{title}</h1>
    <p>报告周期: {period}</p>
    <hr>
    {body_html}
</body>
</html>"""


# ============================================================================
# PDF Export
# ============================================================================

def export_pdf(html_path: str, output_path: str) -> str:
    """
    Convert HTML file to PDF using WeasyPrint.

    Args:
        html_path: Input HTML file path
        output_path: Output PDF file path

    Returns:
        Path to the output PDF file

    Raises:
        ImportError: If WeasyPrint is not installed
        RuntimeError: If PDF generation fails
    """
    try:
        from weasyprint import HTML
    except ImportError:
        raise ImportError(
            "WeasyPrint is required for PDF export. "
            "Install with: pip install weasyprint\n"
            "On macOS also run: brew install cairo pango"
        )

    try:
        html = HTML(filename=html_path)
        html.write_pdf(output_path)
        return str(output_path)
    except Exception as e:
        raise RuntimeError(f"PDF generation failed: {e}")


def export_pdf_with_fallback(html_path: str, output_path: str) -> dict:
    """
    Convert HTML to PDF with graceful fallback.

    Args:
        html_path: Input HTML file path
        output_path: Output PDF file path

    Returns:
        Result dict with 'success', 'path', 'method', 'error' keys
    """
    result = {"success": False, "path": None, "method": None, "error": None}

    # Try WeasyPrint first
    try:
        path = export_pdf(html_path, output_path)
        result.update(success=True, path=path, method="weasyprint")
        return result
    except ImportError as e:
        result["error"] = str(e)
    except RuntimeError as e:
        result["error"] = str(e)

    # Fallback: try wkhtmltopdf
    try:
        import subprocess
        subprocess.run(
            ["wkhtmltopdf", "--version"],
            capture_output=True, check=True,
        )
        subprocess.run(
            ["wkhtmltopdf", html_path, output_path],
            capture_output=True, check=True, timeout=60,
        )
        result.update(success=True, path=output_path, method="wkhtmltopdf")
        return result
    except (FileNotFoundError, subprocess.CalledProcessError) as e:
        if not result["error"]:
            result["error"] = f"wkhtmltopdf not available: {e}"

    # Final fallback: save HTML with .pdf.html extension for manual conversion
    fallback_path = output_path + ".html"
    try:
        import shutil
        shutil.copy2(html_path, fallback_path)
        result.update(
            success=False,
            path=fallback_path,
            method="html_fallback",
            error="No PDF engine available. HTML copy saved for manual conversion.",
        )
    except Exception as e:
        result["error"] = f"All PDF methods failed: {e}"

    return result


# ============================================================================
# Utility
# ============================================================================

def _now_str() -> str:
    from datetime import datetime
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="GeoSDG Report Exporter — Export Markdown reports to HTML/PDF"
    )
    parser.add_argument(
        "--input", required=True,
        help="Input Markdown file path"
    )
    parser.add_argument(
        "--output", "-o",
        help="Output file path (default: same name with new extension)"
    )
    parser.add_argument(
        "--format", "--formats", nargs="+",
        choices=["html", "pdf"],
        default=["html"],
        help="Export format(s) (default: html)"
    )
    parser.add_argument(
        "--title", default="GeoSDG 评估报告",
        help="Report title"
    )
    parser.add_argument(
        "--period", default="",
        help="Report period"
    )
    parser.add_argument(
        "--no-embed-images", action="store_true",
        help="Do not embed images as base64 (use relative paths)"
    )
    parser.add_argument(
        "--output-dir",
        help="Output directory (default: same as input)"
    )

    args = parser.parse_args()

    # Read input
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    md_content = input_path.read_text(encoding="utf-8")
    base_dir = str(input_path.parent)

    output_dir = Path(args.output_dir) if args.output_dir else input_path.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    results = {}

    for fmt in args.format:
        if fmt == "html":
            html_output = args.output or str(output_dir / f"{input_path.stem}.html")
            try:
                path = export_html(
                    md_content=md_content,
                    output_path=html_output,
                    title=args.title,
                    period=args.period,
                    embed_images=not args.no_embed_images,
                    base_dir=base_dir,
                )
                results["html"] = path
                print(f"HTML exported: {path}")
            except Exception as e:
                print(f"Error: HTML export failed: {e}", file=sys.stderr)
                sys.exit(1)

        elif fmt == "pdf":
            # PDF requires HTML as intermediate
            html_path = str(output_dir / f"{input_path.stem}_pdf.html")
            try:
                export_html(
                    md_content=md_content,
                    output_path=html_path,
                    title=args.title,
                    period=args.period,
                    embed_images=not args.no_embed_images,
                    base_dir=base_dir,
                )
            except Exception as e:
                print(f"Error: Intermediate HTML generation failed: {e}", file=sys.stderr)
                sys.exit(1)

            pdf_output = args.output or str(output_dir / f"{input_path.stem}.pdf")
            result = export_pdf_with_fallback(html_path, pdf_output)
            if result["success"]:
                results["pdf"] = result["path"]
                print(f"PDF exported: {result['path']} (method: {result['method']})")
            else:
                print(f"Warning: PDF export failed: {result['error']}", file=sys.stderr)
                if result["path"]:
                    print(f"Fallback HTML saved: {result['path']}", file=sys.stderr)

    # Clean up intermediate HTML for PDF
    if "pdf" in args.format:
        intermediate = output_dir / f"{input_path.stem}_pdf.html"
        if intermediate.exists() and "html" not in args.format:
            intermediate.unlink()

    if not results:
        print("Error: No files exported", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
