#!/usr/bin/env python3
"""
progress_manager.py — Manage data standardization progress across sessions.

Usage:
    python progress_manager.py init <data_dir>
    python progress_manager.py status <data_dir>
    python progress_manager.py update <data_dir> --stage <1-6> --item <desc> --status <done|skip|pending>
    python progress_manager.py resume <data_dir>

Progress file: <data_dir>/.standardize-progress.json
"""

import argparse
import json
import os
import sys
from datetime import datetime

PROGRESS_FILE = ".standardize-progress.json"

STAGES = [
    {"id": 1, "name": "Scan & Inventory", "description": "扫描数据目录，建立文件清单"},
    {"id": 2, "name": "Classify & Rename", "description": "按类别关键词自动分类，确认重命名方案"},
    {"id": 3, "name": "Metadata Check", "description": "检查 CRS/NoData/分辨率一致性"},
    {"id": 4, "name": "Generate Manifest", "description": "生成 manifest.json"},
    {"id": 5, "name": "Validate Structure", "description": "验证目录结构与 manifest 一致性"},
    {"id": 6, "name": "Finalize", "description": "确认最终结构，清理临时文件"},
]


def progress_path(data_dir: str) -> str:
    return os.path.join(data_dir, PROGRESS_FILE)


def load_progress(data_dir: str) -> dict:
    ppath = progress_path(data_dir)
    if os.path.isfile(ppath):
        with open(ppath, "r", encoding="utf-8") as f:
            return json.load(f)
    return None


def save_progress(data_dir: str, progress: dict):
    ppath = progress_path(data_dir)
    with open(ppath, "w", encoding="utf-8") as f:
        json.dump(progress, f, indent=2, ensure_ascii=False)


def cmd_init(data_dir: str):
    """Initialize progress tracking."""
    existing = load_progress(data_dir)
    if existing:
        print(f"Progress file already exists at {progress_path(data_dir)}")
        print(f"  Current stage: {existing.get('current_stage', '?')}")
        return

    progress = {
        "created_at": datetime.now().isoformat(),
        "updated_at": datetime.now().isoformat(),
        "current_stage": 1,
        "stages": {},
    }
    for stage in STAGES:
        progress["stages"][str(stage["id"])] = {
            "name": stage["name"],
            "description": stage["description"],
            "status": "pending",
            "items": [],
        }

    save_progress(data_dir, progress)
    print(f"Initialized progress tracking at {progress_path(data_dir)}")
    print(f"  Total stages: {len(STAGES)}")
    print(f"  Current stage: 1 — {STAGES[0]['name']}")


def cmd_status(data_dir: str):
    """Show current progress status."""
    progress = load_progress(data_dir)
    if not progress:
        print("No progress file found. Run 'init' first.")
        return

    print(f"Data Standardization Progress: {data_dir}")
    print(f"Last updated: {progress.get('updated_at', 'unknown')}")
    print()

    current = progress.get("current_stage", 1)
    for stage in STAGES:
        sid = str(stage["id"])
        stage_data = progress.get("stages", {}).get(sid, {})
        status = stage_data.get("status", "pending")
        marker = {"done": "✅", "skip": "⏭️", "pending": "🔲", "in_progress": "🔄"}.get(status, "❓")
        prefix = "→ " if stage["id"] == current else "  "
        print(f"{prefix}{marker} Stage {stage['id']}: {stage['name']}")

        items = stage_data.get("items", [])
        for item in items:
            item_status = item.get("status", "pending")
            item_marker = {"done": "✓", "skip": "⏭", "pending": "○"}.get(item_status, "○")
            print(f"     {item_marker} {item.get('desc', '')}")

    done_count = sum(1 for s in progress.get("stages", {}).values() if s.get("status") == "done")
    print(f"\nProgress: {done_count}/{len(STAGES)} stages completed")


def cmd_update(data_dir: str, stage: int, item: str | None, status: str):
    """Update progress for a stage or item."""
    progress = load_progress(data_dir)
    if not progress:
        print("No progress file found. Run 'init' first.")
        return

    sid = str(stage)
    if sid not in progress.get("stages", {}):
        print(f"Invalid stage: {stage}")
        return

    if item:
        # Update a specific item within a stage
        items = progress["stages"][sid].get("items", [])
        found = False
        for i in items:
            if i["desc"] == item:
                i["status"] = status
                found = True
                break
        if not found:
            items.append({"desc": item, "status": status})
        progress["stages"][sid]["items"] = items
        progress["stages"][sid]["status"] = "in_progress"
    else:
        # Update the entire stage
        progress["stages"][sid]["status"] = status
        if status == "done" and stage < len(STAGES):
            progress["current_stage"] = stage + 1
        elif status == "in_progress":
            progress["current_stage"] = stage

    progress["updated_at"] = datetime.now().isoformat()
    save_progress(data_dir, progress)
    print(f"Updated stage {stage}: {status}")


def cmd_resume(data_dir: str):
    """Show resume instructions for the current stage."""
    progress = load_progress(data_dir)
    if not progress:
        print("No progress file found. Run 'init' first.")
        return

    current = progress.get("current_stage", 1)
    stage_info = STAGES[current - 1]
    stage_data = progress.get("stages", {}).get(str(current), {})

    print(f"Resume from Stage {current}: {stage_info['name']}")
    print(f"Description: {stage_info['description']}")
    print()

    items = stage_data.get("items", [])
    if items:
        print("Items in this stage:")
        for item in items:
            marker = "✓" if item.get("status") == "done" else "○"
            print(f"  {marker} {item.get('desc', '')}")
    else:
        print("No items tracked yet for this stage.")


def main():
    parser = argparse.ArgumentParser(description="Manage data standardization progress")
    subparsers = parser.add_subparsers(dest="command", required=True)

    p_init = subparsers.add_parser("init", help="Initialize progress tracking")
    p_init.add_argument("data_dir")

    p_status = subparsers.add_parser("status", help="Show progress status")
    p_status.add_argument("data_dir")

    p_update = subparsers.add_parser("update", help="Update progress")
    p_update.add_argument("data_dir")
    p_update.add_argument("--stage", type=int, required=True)
    p_update.add_argument("--item", default=None)
    p_update.add_argument("--status", choices=["done", "skip", "pending", "in_progress"], required=True)

    p_resume = subparsers.add_parser("resume", help="Show resume instructions")
    p_resume.add_argument("data_dir")

    args = parser.parse_args()

    if args.command == "init":
        cmd_init(args.data_dir)
    elif args.command == "status":
        cmd_status(args.data_dir)
    elif args.command == "update":
        cmd_update(args.data_dir, args.stage, args.item, args.status)
    elif args.command == "resume":
        cmd_resume(args.data_dir)


if __name__ == "__main__":
    main()
