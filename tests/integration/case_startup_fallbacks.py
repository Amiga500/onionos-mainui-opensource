# SPDX-License-Identifier: GPL-3.0-only
"""Startup with a damaged SD card.

A missing font exits and names the font instead of printing a stale SDL error.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="startup-fallbacks-", dir=BUILD))
EXE = str(BUILD / "MainUI-dev")


def start(sd, theme, fallback, name):
    output = OUT / (name + ".bmp")
    result = subprocess.run(
        [EXE, "--sd-root", str(sd), "--theme", str(theme), "--fallback", str(fallback),
         "--systems", "--snapshot", str(output)],
        cwd=ROOT, capture_output=True, text=True, timeout=30)
    return result, output


# No font anywhere: a clean exit that names the font, not an SDL error.
fontless = OUT / "fontless"
shutil.copytree(ONION_THEME / "skin", fontless / "skin")
shutil.copy(ONION_THEME / "config.json", fontless / "config.json")
empty = OUT / "empty"
(empty / "Emu").mkdir(parents=True)
result, output = start(empty, fontless, fontless, "fontless")
assert result.returncode == 3, (result.returncode, result.stderr)
assert "Cannot open font" in result.stderr, result.stderr
assert "colorkey" not in result.stderr, result.stderr
assert not output.exists()
print("startup_fallbacks: ok")
