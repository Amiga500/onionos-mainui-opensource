# SPDX-License-Identifier: GPL-3.0-only
"""device/MainUI-test-wrapper.sh against a fake SD tree with fake launchers.

Covers the stock switches (DISABLED, missing binary, two starts without a first
frame), best-effort logging and the last-resort path when the stock backup is
missing. No MainUI build is needed: both launchers are small shell scripts that
record which one ran.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from env import BUILD  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / "device/MainUI-test-wrapper.sh"
SHELL = shutil.which("sh") or "/bin/sh"
WORK = Path(tempfile.mkdtemp(prefix="device-wrapper-", dir=BUILD if BUILD.is_dir() else None))
SD, TMP, RAN = WORK / "sd", WORK / "tmp", WORK / "ran"
TEST = SD / ".tmp_update/mainui-test"
LOGS = SD / ".tmp_update/logs"
OPEN_ARGS = f"--sd-root {SD} --device real --handoff-dir {TMP}"


def script(path, body):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
    path.chmod(0o755)


def run(draws=True):
    """Start the wrapper; return what ran ('stock' or 'open:<args>') and stdout."""
    if RAN.exists():
        RAN.unlink()
    env = dict(os.environ, MAINUI_WRAPPER_SD=str(SD), MAINUI_WRAPPER_TMP=str(TMP), RAN=str(RAN))
    if draws:
        env["FAKE_DRAWS"] = "1"
    result = subprocess.run([SHELL, str(WRAPPER)], env=env, capture_output=True, text=True,
                            timeout=30)
    ran = RAN.read_text().strip() if RAN.exists() else ""
    return ran, result.stdout


def reboot():
    for name in TMP.iterdir():
        name.unlink()


(SD / "miyoo/app").mkdir(parents=True)
(SD / ".tmp_update/config").mkdir(parents=True)
TMP.mkdir()
script(TEST / "stock/MainUI", 'echo stock > "$RAN"\n')
# The fake Open MainUI removes the start marker like the real one after its first frame.
script(TEST / "MainUI", 'echo "open:$*" > "$RAN"\necho out\n'
       '[ -n "$FAKE_DRAWS" ] && rm -f "$MAINUI_START_MARKER"\nexit 0\n')

# Normal start, silent without .logging.
assert run() == ("open:" + OPEN_ARGS, ""), run()
assert not (TMP / "open-mainui.starting").exists()

# DISABLED selects stock; removing it resumes Open MainUI.
(TEST / "DISABLED").touch()
assert run()[0] == "stock"
(TEST / "DISABLED").unlink()
assert run()[0] == "open:" + OPEN_ARGS

# Two starts without a first frame: stock until reboot.
assert run(draws=False)[0].startswith("open:")
assert run(draws=False)[0].startswith("open:")
assert run()[0] == "stock"
assert run()[0] == "stock", "fallback must last until reboot"
reboot()
assert run()[0] == "open:" + OPEN_ARGS

# A single start without a first frame is not enough.
run(draws=False)
assert run()[0].startswith("open:")

# Logging: output appended to the log, stdout silent.
(SD / ".tmp_update/config/.logging").touch()
ran, stdout = run()
assert ran.startswith("open:") and stdout == ""
assert "out" in (LOGS / "MainUI.log").read_text()

# Rotation at 1 MiB keeps one previous log.
(LOGS / "MainUI.log").write_bytes(b"x" * 1048576)
run()
assert (LOGS / "MainUI.log.1").stat().st_size == 1048576
assert (LOGS / "MainUI.log").stat().st_size < 1048576

# Unwritable logs must not stop the launcher (not meaningful as root).
if os.geteuid() != 0:
    LOGS.chmod(0o500)
    try:
        ran, stdout = run()
    finally:
        LOGS.chmod(0o700)
    assert ran == "open:" + OPEN_ARGS, ran
(SD / ".tmp_update/config/.logging").unlink()

# Missing Open MainUI binary: stock.
(TEST / "MainUI").rename(WORK / "saved")
assert run()[0] == "stock"
(WORK / "saved").rename(TEST / "MainUI")

# DISABLED but no stock backup: Open MainUI as the last resort.
(TEST / "stock/MainUI").unlink()
(TEST / "DISABLED").touch()
assert run()[0] == "open:" + OPEN_ARGS

shutil.rmtree(WORK)
print("device_wrapper: ok")
