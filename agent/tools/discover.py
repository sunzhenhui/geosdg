"""
Plugin discovery tool — dynamically discovers available GeoSDG CLI plugins.

This module provides functions to discover both built-in and external plugins
by invoking `geosdg-cli list-plugins` and parsing the output. It also supports
reading plugin.json files directly from plugin directories.

Usage:
    from agent.tools.discover import discover_plugins, get_known_tools

    plugins = discover_plugins()
    tools = get_known_tools()  # set of all plugin names + built-in commands
"""

import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Set


# ============================================================================
# Constants
# ============================================================================

DEFAULT_CLI_PATH = "geosdg-cli"
DEFAULT_PLUGIN_DIRS = [
    "plugins",
    "../plugins",
]

# Built-in CLI commands that are always available (not plugin-based)
BUILTIN_CLI_COMMANDS = {
    "ca-pg", "ca-markov", "ca-precision", "ca-simulate",
    "sdg-predict", "sdg-infra-simulate",
    "correlation", "t-test",
    "check", "resample", "normalize", "reclass",
    "detect-change", "compress",
    "pipeline",
    "demo", "help", "version",
    "list-plugins", "plugin-info",
}


# ============================================================================
# Plugin discovery
# ============================================================================

def discover_plugins(cli_path: str = DEFAULT_CLI_PATH,
                     plugin_dirs: Optional[List[str]] = None,
                     timeout: int = 10) -> List[Dict[str, Any]]:
    """
    Discover all available plugins by running `geosdg-cli list-plugins`.

    @param cli_path Path to the geosdg-cli executable
    @param plugin_dirs Additional plugin search directories
    @param timeout Timeout in seconds for the CLI command
    @return List of plugin info dictionaries with keys:
            name, version, author, description, category, status, is_builtin
    """
    cmd = [cli_path, "list-plugins"]
    if plugin_dirs:
        for d in plugin_dirs:
            cmd.extend(["--plugin-dir", d])

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False
        )
    except FileNotFoundError:
        print(f"Warning: geosdg-cli not found at '{cli_path}'", file=sys.stderr)
        return _discover_from_dirs(plugin_dirs or DEFAULT_PLUGIN_DIRS)
    except subprocess.TimeoutExpired:
        print(f"Warning: geosdg-cli list-plugins timed out after {timeout}s", file=sys.stderr)
        return []

    if result.returncode != 0:
        print(f"Warning: geosdg-cli list-plugins failed: {result.stderr}", file=sys.stderr)
        return _discover_from_dirs(plugin_dirs or DEFAULT_PLUGIN_DIRS)

    return _parse_list_plugins_output(result.stdout)


def _parse_list_plugins_output(output: str) -> List[Dict[str, Any]]:
    """
    Parse the output of `geosdg-cli list-plugins` into structured data.

    Expected output format per line:
        [builtin]  name    — description (v1.0, by Author)
        [plugin]   name    — description (v0.1.0, by Author)
        [error]    name    (error message)

    @param output stdout from geosdg-cli list-plugins
    @return List of plugin info dictionaries
    """
    plugins = []
    for line in output.strip().split("\n"):
        line = line.strip()
        if not line:
            continue

        plugin = _parse_plugin_line(line)
        if plugin:
            plugins.append(plugin)

    return plugins


def _parse_plugin_line(line: str) -> Optional[Dict[str, Any]]:
    """
    Parse a single line from list-plugins output.

    @param line A single line of output
    @return Plugin info dict or None if unparseable
    """
    # Match: [tag]  name  — description (vX.Y.Z, by Author)
    # or:     [tag]  name  (error message)
    match = re.match(
        r'^\[(\w+)\]\s+(\S+)(?:\s+—\s+(.+?))?(?:\s+\(v([^,]+),\s+by\s+(.+?)\))?(?:\s+\((.+)\))?$',
        line
    )
    if not match:
        return None

    tag, name, desc, version, author, error = match.groups()

    plugin = {
        "name": name,
        "status": tag,
        "is_builtin": tag == "builtin",
        "description": desc or "",
        "version": version or "unknown",
        "author": author or "unknown",
        "category": "",
        "error": error if tag == "error" else None,
    }
    return plugin


def _discover_from_dirs(plugin_dirs: List[str]) -> List[Dict[str, Any]]:
    """
    Fallback: discover plugins by scanning plugin directories for plugin.json files.

    @param plugin_dirs List of plugin directory paths to scan
    @return List of plugin info dictionaries
    """
    plugins = []
    for d in plugin_dirs:
        dir_path = Path(d)
        if not dir_path.exists() or not dir_path.is_dir():
            continue

        for entry in dir_path.iterdir():
            if not entry.is_dir():
                continue
            json_path = entry / "plugin.json"
            if not json_path.exists():
                continue

            try:
                with open(json_path, "r", encoding="utf-8") as f:
                    desc = json.load(f)
                plugins.append({
                    "name": desc.get("name", entry.name),
                    "status": "plugin",
                    "is_builtin": False,
                    "description": desc.get("description", ""),
                    "version": desc.get("version", "unknown"),
                    "author": desc.get("author", "unknown"),
                    "category": desc.get("category", ""),
                    "error": None,
                })
            except (json.JSONDecodeError, OSError) as e:
                plugins.append({
                    "name": entry.name,
                    "status": "error",
                    "is_builtin": False,
                    "description": "",
                    "version": "unknown",
                    "author": "unknown",
                    "category": "",
                    "error": str(e),
                })

    return plugins


# ============================================================================
# Tool name resolution
# ============================================================================

def get_known_tools(cli_path: str = DEFAULT_CLI_PATH,
                    plugin_dirs: Optional[List[str]] = None) -> Set[str]:
    """
    Get the complete set of known tool names (built-in commands + plugins).

    This function is used by the pipeline schema validator to dynamically
    determine which tool names are valid in pipeline step configurations.

    @param cli_path Path to the geosdg-cli executable
    @param plugin_dirs Additional plugin search directories
    @return Set of all valid tool names
    """
    tools = set(BUILTIN_CLI_COMMANDS)
    plugins = discover_plugins(cli_path, plugin_dirs)
    for plugin in plugins:
        if plugin["status"] != "error":
            tools.add(plugin["name"])
    return tools


def get_plugin_info(plugin_name: str,
                    cli_path: str = DEFAULT_CLI_PATH,
                    plugin_dirs: Optional[List[str]] = None) -> Optional[Dict[str, Any]]:
    """
    Get detailed information about a specific plugin.

    @param plugin_name Name of the plugin
    @param cli_path Path to the geosdg-cli executable
    @param plugin_dirs Additional plugin search directories
    @return Plugin info dict or None if not found
    """
    plugins = discover_plugins(cli_path, plugin_dirs)
    for plugin in plugins:
        if plugin["name"] == plugin_name:
            return plugin
    return None


# ============================================================================
# CLI entry point
# ============================================================================

def main():
    """CLI entry point for standalone execution."""
    import argparse
    parser = argparse.ArgumentParser(description="Discover GeoSDG plugins")
    parser.add_argument("--cli", default=DEFAULT_CLI_PATH, help="Path to geosdg-cli")
    parser.add_argument("--plugin-dir", action="append", help="Additional plugin directory")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    plugins = discover_plugins(args.cli, args.plugin_dir)

    if args.json:
        print(json.dumps(plugins, indent=2, ensure_ascii=False))
    else:
        if not plugins:
            print("No plugins found.")
            return

        print(f"Found {len(plugins)} plugin(s):\n")
        for p in plugins:
            tag = p["status"]
            line = f"  [{tag}] {p['name']}"
            if p["description"]:
                line += f"  — {p['description']}"
            if p["version"] != "unknown":
                line += f"  (v{p['version']}, by {p['author']})"
            if p["error"]:
                line += f"  ({p['error']})"
            print(line)

        print(f"\nKnown tools: {', '.join(sorted(get_known_tools(args.cli, args.plugin_dir)))}")


if __name__ == "__main__":
    main()
