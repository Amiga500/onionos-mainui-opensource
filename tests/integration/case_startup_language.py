# SPDX-License-Identifier: GPL-3.0-only
"""The language saved in system.json is used from startup, not only after it is
chosen again in Settings. Only a plain file name is accepted."""
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="startup-language-", dir=BUILD))
SD = OUT / "sd"
(SD / "Emu").mkdir(parents=True)
(SD / "miyoo/app/lang").mkdir(parents=True)
(SD / "miyoo/app/lang/xx.lang").write_text(
    "\ufeff" + json.dumps({"lang": "Test", "15": "XXXXXXXXXXXX"}), encoding="utf-8")
(OUT / "evil.lang").write_text(json.dumps({"lang": "Evil", "15": "EEEEEEEEEEEE"}))


def settings(language):
    (SD / "system.json").write_text(json.dumps({"language": language}))
    output = OUT / f"{len(list(OUT.glob('*.bmp')))}.bmp"
    result = subprocess.run(
        [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
         "--fallback", str(ONION_THEME), "--input", "RRRE", "--snapshot", str(output)],
        cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    # The Settings title row is the only place label 15 appears.
    return Image.open(output).convert("RGB").crop((0, 0, 640, 60)).tobytes(), result.stderr


english, _ = settings("en.lang")
translated, _ = settings("xx.lang")
assert translated != english
for bad in ("../../evil.lang", "missing.lang", "notalang.txt"):
    title, stderr = settings(bad)
    assert title == english, bad
    assert "using built-in English" in stderr, (bad, stderr)
print("startup_language: ok")
