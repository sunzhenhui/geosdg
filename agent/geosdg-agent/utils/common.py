"""共享常量与配置（对标 PEACE/utils/common.py）."""

from __future__ import annotations

import os
from pathlib import Path

# ============================================================================
# 全局开关
# ============================================================================

# 是否打印中间过程（对标 PEACE common.echo）
echo: bool = True

# 数据集/场景标记（对标 PEACE common.dataset_source）
# 可选："cn"（中文语境） / "en"（英文语境）
language: str = "cn"

# ============================================================================
# 路径
# ============================================================================

PROJECT_ROOT: Path = Path(__file__).resolve().parents[3]
"""工程根目录（geosdg 仓库根）."""

DATA_DIR: Path = PROJECT_ROOT / "data"
"""数据根目录，存放 LUCC/人口/夜光等栅格."""

CACHE_DIR: Path = PROJECT_ROOT / ".cache" / "geosdg-agent"
"""Agent 中间产物缓存（分区裁剪、meta.json 等）."""

CLI_BINARY: Path = PROJECT_ROOT / "cli" / "build" / "bin" / "geosdg-cli"
"""geosdg-cli 可执行文件路径（真实计算指标时调用）."""

# ============================================================================
# LLM 配置（OpenAI 兼容接口；未配置 API Key 时自动走 mock）
# ============================================================================

LLM_API_KEY: str = os.environ.get("GEOSDG_LLM_API_KEY", "")
"""LLM API Key；为空则 api.chat 走 mock（骨架仍可跑）."""

LLM_BASE_URL: str = os.environ.get(
    "GEOSDG_LLM_BASE_URL", "https://api.openai.com/v1"
)
"""OpenAI 兼容端点根地址（混元/通义/文心/DeepSeek 等均可通过其兼容端点接入）."""

LLM_MODEL: str = os.environ.get("GEOSDG_LLM_MODEL", "gpt-4o-mini")
"""默认模型名（专家调用时传入的 mock-* 占位名会被此默认值覆盖）."""

LLM_TEMPERATURE: float = float(os.environ.get("GEOSDG_LLM_TEMPERATURE", "0.2"))
"""采样温度."""

LLM_MAX_TOKENS: int = int(os.environ.get("GEOSDG_LLM_MAX_TOKENS", "1024"))
"""单次回复最大 token 数."""

LLM_TIMEOUT: float = float(os.environ.get("GEOSDG_LLM_TIMEOUT", "60"))
"""请求超时（秒）."""


def llm_enabled() -> bool:
    """是否已配置真实 LLM（有 API Key 才启用，否则走 mock）."""
    return bool(LLM_API_KEY)


def ensure_cache_dir(subdir: str = "") -> Path:
    """确保缓存目录存在并返回."""
    target = CACHE_DIR / subdir if subdir else CACHE_DIR
    target.mkdir(parents=True, exist_ok=True)
    return target


# ============================================================================
# 内容安全过滤桩（对标 PEACE common.rai_filter）
# ============================================================================

def rai_filter(question: str) -> bool:
    """Responsible AI 过滤桩：True 表示应拒答.

    骨架阶段永远放行，后续可接入敏感词/政治红线检测.
    """
    _ = question  # unused
    return False
