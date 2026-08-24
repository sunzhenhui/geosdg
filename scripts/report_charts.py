#!/usr/bin/env python3
"""
GeoSDG Report Charts — SDG 评估可视化图表生成工具

生成雷达图、柱状图、趋势图、子区域对比图、优先区域空间分布图 PNG，
嵌入 Markdown 报告供用户直接查看。

配色规范：维度配色使用联合国官方三维度色系（经济红#C5192D/社会蓝#00558A/环境绿#3F7E44），
各 SDG 指标使用联合国官方配色，优先等级色阶 0级#EEEEEE→6级#D73027。

用法 / Usage:
    python report_charts.py --type radar --data '{"economy":78.3,"society":65.4,"environment":54.3}' --output radar.png
    python report_charts.py --type bar --data '{"indicators":[...]}' --output bar.png
    python report_charts.py --type trend --data '{"periods":[...]}' --output trend.png
    python report_charts.py --type subregion --data '{"regions":[...]}' --output subregion.png
    python report_charts.py --type priority-map --data '{"ranking_tif":"...","stats":{...}}' --output priority.png
    python report_charts.py --type cross-dimension --data '{"indicators":[...]}' --output cross_dim.png
    python report_charts.py --type priority-trend --data '{"periods":[...]}' --output priority_trend.png

依赖 / Dependencies:
    Python 3.9+, matplotlib, numpy
    priority-map 类型额外需要: GDAL (python-gdal / rasterio)
"""

import argparse
import json
import sys
import os

# ── 降级策略：matplotlib 不可用时输出占位文本 ──
try:
    import matplotlib
    matplotlib.use("Agg")  # Non-interactive backend
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import matplotlib.font_manager as fm
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

# ── CJK 字体回退：确保中文标签正确渲染 ──
# macOS: PingFang SC / Heiti SC; Windows: SimHei / Microsoft YaHei; Linux: Noto Sans CJK
CJK_FONT_CANDIDATES = [
    "PingFang SC", "Heiti SC", "Heiti TC",          # macOS
    "SimHei", "Microsoft YaHei", "SimSun",           # Windows
    "Noto Sans CJK SC", "Noto Sans CJK", "WenQuanYi Micro Hei",  # Linux
]

def _setup_cjk_font():
    """尝试设置 CJK 字体，回退到系统默认字体"""
    if not HAS_MATPLOTLIB:
        return
    available_fonts = set(f.name for f in fm.fontManager.ttflist)
    for cjk_font in CJK_FONT_CANDIDATES:
        if cjk_font in available_fonts:
            plt.rcParams["font.family"] = [cjk_font, "sans-serif"]
            plt.rcParams["axes.unicode_minus"] = False  # Fix minus sign rendering
            return
    # No CJK font found — labels may render as boxes, but script still works
    plt.rcParams["axes.unicode_minus"] = False

_setup_cjk_font()

# ── SDG 官方配色 (全部 17 个 SDG, UN SDG Guidelines) ──
# 当前 GeoSDG 已实现 10 个指标 (2,3,4,6,7,9,11,13,14,15)，
# 其余 7 个 (1,5,8,10,12,16,17) 为未来指标拓展预留配色。
SDG_COLORS = {
    "1":  "#E524A2",  # 无贫穷
    "2":  "#DDA63A",  # 零饥饿
    "3":  "#4C9F38",  # 良好健康与福祉
    "4":  "#C5192D",  # 优质教育
    "5":  "#FF3A21",  # 性别平等
    "6":  "#26BDE2",  # 清洁饮水与卫生设施
    "7":  "#FCC30B",  # 经济适用的清洁能源
    "8":  "#A21942",  # 体面工作和经济增长
    "9":  "#FD6925",  # 产业、创新与基础设施
    "10": "#DD1367",  # 减少不平等
    "11": "#FD9D24",  # 可持续城市和社区
    "12": "#BF8B2E",  # 负责任消费和生产
    "13": "#3F7E44",  # 气候行动
    "14": "#0A97D9",  # 水下生物
    "15": "#56C02B",  # 陆地生物
    "16": "#00689D",  # 和平、正义与强大机构
    "17": "#19486A",  # 促进目标实现的伙伴关系
}

SDG_LABELS = {
    "1":  "SDG 1 无贫穷",
    "2":  "SDG 2 零饥饿",
    "3":  "SDG 3 良好健康与福祉",
    "4":  "SDG 4 优质教育",
    "5":  "SDG 5 性别平等",
    "6":  "SDG 6 清洁饮水与卫生设施",
    "7":  "SDG 7 经济适用的清洁能源",
    "8":  "SDG 8 体面工作和经济增长",
    "9":  "SDG 9 产业、创新与基础设施",
    "10": "SDG 10 减少不平等",
    "11": "SDG 11 可持续城市和社区",
    "12": "SDG 12 负责任消费和生产",
    "13": "SDG 13 气候行动",
    "14": "SDG 14 水下生物",
    "15": "SDG 15 陆地生物",
    "16": "SDG 16 和平、正义与强大机构",
    "17": "SDG 17 促进目标实现的伙伴关系",
}

# ── 维度配色（联合国官方三维度色系）──
# 参考 UN SDG Color Wheel Infographic：经济=红色、社会=蓝色、环境=绿色
# https://unsdg.un.org/resources/guidelines-use-sdg-logo-including-colour-wheel-and-17-icons
DIMENSION_COLORS = {
    "economy": "#C5192D",     # UN 红 — 经济维度
    "society": "#00558A",     # UN 蓝 — 社会维度
    "environment": "#3F7E44", # UN 绿 — 环境维度
}

DIMENSION_LABELS = {
    "economy": "经济维度",
    "society": "社会维度",
    "environment": "环境维度",
}

# ── 优先等级色阶 (0-6) ──
PRIORITY_LEVEL_COLORS = [
    "#EEEEEE",  # 0 - 非优先
    "#FFEDA0",  # 1
    "#FEB24C",  # 2
    "#FD8D3C",  # 3
    "#FC4E2A",  # 4
    "#E31A1C",  # 5
    "#D73027",  # 6 - 极优先
]

PRIORITY_LEVEL_LABELS = [
    "0级（非优先）", "1级", "2级", "3级", "4级", "5级", "6级（极优先）"
]


def _fail_placeholder(output_path, chart_type, reason=""):
    """降级输出：matplotlib 不可用时生成占位文本"""
    placeholder = output_path.replace(".png", "_placeholder.txt")
    msg = f"[图表生成失败] type={chart_type}"
    if reason:
        msg += f" reason={reason}"
    with open(placeholder, "w", encoding="utf-8") as f:
        f.write(msg + "\n")
    print(json.dumps({"output_path": placeholder, "status": "failed", "reason": reason}))
    return 1


def generate_radar(data, output, title=""):
    """生成三维度雷达图 PNG（UN 官方三维度配色）"""
    if not HAS_MATPLOTLIB:
        return _fail_placeholder(output, "radar", "matplotlib not available")

    dims = ["economy", "society", "environment"]
    values = [data.get(d, 0) for d in dims]
    labels = [DIMENSION_LABELS.get(d, d) for d in dims]
    colors = [DIMENSION_COLORS[d] for d in dims]

    # Close the radar polygon
    values_closed = values + [values[0]]
    angles = np.linspace(0, 2 * np.pi, len(dims), endpoint=False).tolist()
    angles_closed = angles + [angles[0]]

    fig, ax = plt.subplots(figsize=(6, 6), subplot_kw=dict(polar=True))
    # Use UN dimension colors: fill with blended dimension color, line with dark neutral
    ax.fill(angles_closed, values_closed, alpha=0.20, color="#C5192D")
    ax.plot(angles_closed, values_closed, "o-", linewidth=2, color="#333333")

    # Add score labels with dimension colors
    for angle, val, color in zip(angles, values, colors):
        ax.annotate(f"{val:.1f}", xy=(angle, val), fontsize=11,
                    ha="center", va="bottom", fontweight="bold", color=color)

    ax.set_xticks(angles)
    ax.set_xticklabels(labels, fontsize=12)
    # Color each axis label by its dimension color (UN official)
    for i, color in enumerate(colors):
        ax.get_xticklabels()[i].set_color(color)
    ax.set_ylim(0, 100)
    ax.set_yticks([20, 40, 60, 80, 100])
    ax.set_yticklabels(["20", "40", "60", "80", "100"], fontsize=8, color="grey")

    if title:
        ax.set_title(title, fontsize=14, pad=20)

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"output_path": output, "status": "success"}))
    return 0


def generate_bar(data, output, title=""):
    """生成各指标水平柱状图 PNG"""
    if not HAS_MATPLOTLIB:
        return _fail_placeholder(output, "bar", "matplotlib not available")

    indicators = data.get("indicators", [])
    if not indicators:
        return _fail_placeholder(output, "bar", "no indicator data")

    # Sort by score descending
    indicators_sorted = sorted(indicators, key=lambda x: x.get("score", 0))
    names = [ind.get("name", "") for ind in indicators_sorted]
    scores = [ind.get("score", 0) for ind in indicators_sorted]
    dimensions = [ind.get("dimension", "economy") for ind in indicators_sorted]
    sdg_nums = [ind.get("sdg", "") for ind in indicators_sorted]

    # Color by SDG official color
    bar_colors = []
    for sdg in sdg_nums:
        bar_colors.append(SDG_COLORS.get(str(sdg), "#999999"))

    fig, ax = plt.subplots(figsize=(10, max(6, len(names) * 0.4)))
    y_pos = np.arange(len(names))
    bars = ax.barh(y_pos, scores, color=bar_colors, edgecolor="#333333", linewidth=0.5, height=0.7)

    # Add score labels
    for bar, score in zip(bars, scores):
        ax.text(bar.get_width() + 1, bar.get_y() + bar.get_height() / 2,
                f"{score:.1f}", va="center", fontsize=9)

    ax.set_yticks(y_pos)
    ax.set_yticklabels(names, fontsize=9)
    ax.set_xlim(0, 110)
    ax.set_xlabel("得分 (0-100)", fontsize=11)
    if title:
        ax.set_title(title, fontsize=14)

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"output_path": output, "status": "success"}))
    return 0


def generate_trend(data, output, title=""):
    """生成时间变化趋势折线图 PNG"""
    if not HAS_MATPLOTLIB:
        return _fail_placeholder(output, "trend", "matplotlib not available")

    periods = data.get("periods", [])
    if not periods:
        return _fail_placeholder(output, "trend", "no period data")

    years = [p.get("year", 0) for p in periods]
    composite = [p.get("composite", 0) for p in periods]
    economy = [p.get("economy", 0) for p in periods]
    society = [p.get("society", 0) for p in periods]
    environment = [p.get("environment", 0) for p in periods]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(years, composite, "o-", label="综合得分", linewidth=2.5, color="#333333", markersize=7)
    ax.plot(years, economy, "s--", label="经济维度", linewidth=1.5, color=DIMENSION_COLORS["economy"], markersize=5)
    ax.plot(years, society, "^--", label="社会维度", linewidth=1.5, color=DIMENSION_COLORS["society"], markersize=5)
    ax.plot(years, environment, "D--", label="环境维度", linewidth=1.5, color=DIMENSION_COLORS["environment"], markersize=5)

    # Add value labels for composite line
    for x, y in zip(years, composite):
        ax.annotate(f"{y:.1f}", xy=(x, y), fontsize=9, ha="center", va="bottom")

    ax.set_ylim(0, 100)
    ax.set_xlabel("年份", fontsize=11)
    ax.set_ylabel("得分", fontsize=11)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    if title:
        ax.set_title(title, fontsize=14)

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"output_path": output, "status": "success"}))
    return 0


def generate_subregion(data, output, title=""):
    """生成子区域对比分组柱状图 PNG"""
    if not HAS_MATPLOTLIB:
        return _fail_placeholder(output, "subregion", "matplotlib not available")

    regions = data.get("regions", [])
    if not regions:
        return _fail_placeholder(output, "subregion", "no region data")

    # Limit to Top 10 + Bottom 5 if > 20 regions
    if len(regions) > 20:
        regions_sorted = sorted(regions, key=lambda x: x.get("composite", 0), reverse=True)
        regions = regions_sorted[:10] + regions_sorted[-5:]

    names = [r.get("name", "") for r in regions]
    economy = [r.get("economy", 0) for r in regions]
    society = [r.get("society", 0) for r in regions]
    environment = [r.get("environment", 0) for r in regions]

    x = np.arange(len(names))
    width = 0.25

    fig, ax = plt.subplots(figsize=(max(10, len(names) * 1.2), 6))
    ax.bar(x - width, economy, width, label="经济", color=DIMENSION_COLORS["economy"], edgecolor="#333333", linewidth=0.5)
    ax.bar(x, society, width, label="社会", color=DIMENSION_COLORS["society"], edgecolor="#333333", linewidth=0.5)
    ax.bar(x + width, environment, width, label="环境", color=DIMENSION_COLORS["environment"], edgecolor="#333333", linewidth=0.5)

    ax.set_xticks(x)
    ax.set_xticklabels(names, fontsize=9, rotation=30, ha="right")
    ax.set_ylim(0, 110)
    ax.set_ylabel("得分", fontsize=11)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3, axis="y")
    if title:
        ax.set_title(title, fontsize=14)

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"output_path": output, "status": "success"}))
    return 0


def generate_priority_map(data, output, title=""):
    """生成优先区域空间分布图 PNG"""
    if not HAS_MATPLOTLIB:
        return _fail_placeholder(output, "priority-map", "matplotlib not available")

    ranking_tif = data.get("ranking_tif", "")
    stats = data.get("stats", {})

    if not ranking_tif or not os.path.exists(ranking_tif):
        return _fail_placeholder(output, "priority-map", f"ranking tif not found: {ranking_tif}")

    # Try to read GeoTIFF with GDAL Python bindings or rasterio
    raster_data = None
    try:
        from osgeo import gdal
        ds = gdal.Open(ranking_tif, gdal.GA_ReadOnly)
        if ds:
            band = ds.GetRasterBand(1)
            raster_data = band.ReadAsArray()
            ds = None
    except ImportError:
        pass

    if raster_data is None:
        try:
            import rasterio
            with rasterio.open(ranking_tif) as src:
                raster_data = src.read(1)
        except ImportError:
            return _fail_placeholder(output, "priority-map",
                                     "GDAL Python bindings and rasterio not available")

    if raster_data is None:
        return _fail_placeholder(output, "priority-map", "failed to read raster data")

    # Create color map (0-6)
    from matplotlib.colors import ListedColormap
    cmap = ListedColormap(PRIORITY_LEVEL_COLORS)

    fig, ax = plt.subplots(figsize=(8, 6))
    im = ax.imshow(raster_data, cmap=cmap, vmin=0, vmax=6)

    # Add colorbar as legend
    cbar = fig.colorbar(im, ax=ax, ticks=range(7), shrink=0.8)
    cbar.ax.set_yticklabels(PRIORITY_LEVEL_LABELS, fontsize=8)

    ax.set_axis_off()
    if title:
        ax.set_title(title, fontsize=14)

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"output_path": output, "status": "success"}))
    return 0


def generate_cross_dimension(data, output, title=""):
    """生成跨维度散点图 PNG — 展示经济/社会/环境维度间的关联性"""
    if not HAS_MATPLOTLIB:
        return _fail_placeholder(output, "cross-dimension", "matplotlib not available")

    indicators = data.get("indicators", [])
    if not indicators:
        return _fail_placeholder(output, "cross-dimension", "no indicator data")

    # Extract dimension scores per indicator
    points = []
    for ind in indicators:
        economy = ind.get("economy", ind.get("economy_score"))
        society = ind.get("society", ind.get("society_score"))
        environment = ind.get("environment", ind.get("environment_score"))
        name = ind.get("name", ind.get("code", ""))
        sdg = str(ind.get("sdg", ""))

        # Need at least two dimensions to plot
        if economy is not None and environment is not None:
            points.append({
                "x": economy, "y": environment,
                "x_label": "经济维度", "y_label": "环境维度",
                "name": name, "sdg": sdg,
            })
        elif economy is not None and society is not None:
            points.append({
                "x": economy, "y": society,
                "x_label": "经济维度", "y_label": "社会维度",
                "name": name, "sdg": sdg,
            })
        elif society is not None and environment is not None:
            points.append({
                "x": society, "y": environment,
                "x_label": "社会维度", "y_label": "环境维度",
                "name": name, "sdg": sdg,
            })

    if not points:
        return _fail_placeholder(output, "cross-dimension", "insufficient dimension data")

    fig, ax = plt.subplots(figsize=(8, 6))

    # Plot each point with SDG color
    for pt in points:
        color = SDG_COLORS.get(pt["sdg"], "#999999")
        ax.scatter(pt["x"], pt["y"], c=color, s=100, zorder=5, edgecolors="#333", linewidth=0.5)
        ax.annotate(pt["name"], (pt["x"], pt["y"]),
                    fontsize=8, ha="left", va="bottom",
                    xytext=(5, 5), textcoords="offset points")

    # Add diagonal reference line (ideal: both dimensions high)
    ax.plot([0, 100], [0, 100], "--", color="#CCCCCC", linewidth=1, alpha=0.5)

    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.set_xlabel(points[0]["x_label"], fontsize=11)
    ax.set_ylabel(points[0]["y_label"], fontsize=11)
    ax.grid(True, alpha=0.3)

    if title:
        ax.set_title(title, fontsize=14)

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"output_path": output, "status": "success"}))
    return 0


def generate_priority_trend(data, output, title=""):
    """生成优先区域面积变化趋势图 PNG — 堆叠面积图展示各优先级面积随时间变化"""
    if not HAS_MATPLOTLIB:
        return _fail_placeholder(output, "priority-trend", "matplotlib not available")

    periods = data.get("periods", [])
    if not periods:
        return _fail_placeholder(output, "priority-trend", "no period data")

    years = [p.get("year", 0) for p in periods]

    # Extract area by priority level for each period
    # Expected format: {"periods": [{"year": 2010, "levels": {"0": 1000, "1": 500, ...}}, ...]}
    level_keys = [str(i) for i in range(7)]
    level_areas = {lk: [] for lk in level_keys}

    for p in periods:
        levels = p.get("levels", {})
        for lk in level_keys:
            level_areas[lk].append(levels.get(lk, 0))

    fig, ax = plt.subplots(figsize=(10, 6))

    # Stacked area chart
    bottom = np.zeros(len(years))
    for i, lk in enumerate(level_keys):
        values = np.array(level_areas[lk])
        if values.sum() > 0:
            ax.fill_between(years, bottom, bottom + values,
                            color=PRIORITY_LEVEL_COLORS[i],
                            label=PRIORITY_LEVEL_LABELS[i], alpha=0.8)
            bottom += values

    ax.set_xlabel("年份", fontsize=11)
    ax.set_ylabel("面积 (km²)", fontsize=11)
    ax.legend(fontsize=8, loc="upper left", ncol=2)
    ax.grid(True, alpha=0.3, axis="y")

    if title:
        ax.set_title(title, fontsize=14)

    plt.tight_layout()
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"output_path": output, "status": "success"}))
    return 0


# ── 主入口 / Main entry ──

CHART_GENERATORS = {
    "radar": generate_radar,
    "bar": generate_bar,
    "trend": generate_trend,
    "subregion": generate_subregion,
    "priority-map": generate_priority_map,
    "cross-dimension": generate_cross_dimension,
    "priority-trend": generate_priority_trend,
}


def main():
    parser = argparse.ArgumentParser(description="GeoSDG Report Charts Generator")
    parser.add_argument("--type", required=True, choices=CHART_GENERATORS.keys(),
                        help="Chart type")
    parser.add_argument("--data", required=True,
                        help="Chart data in JSON format")
    parser.add_argument("--output", required=True,
                        help="Output PNG file path")
    parser.add_argument("--title", default="",
                        help="Chart title (optional)")

    args = parser.parse_args()

    try:
        data = json.loads(args.data)
    except json.JSONDecodeError as e:
        print(json.dumps({"output_path": args.output, "status": "failed", "reason": f"Invalid JSON: {e}"}))
        return 1

    # Ensure output directory exists
    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    generator = CHART_GENERATORS[args.type]
    return generator(data, args.output, args.title)


if __name__ == "__main__":
    sys.exit(main())
