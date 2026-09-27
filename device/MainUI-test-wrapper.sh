#!/bin/sh
# Runs Open MainUI in the Onion MainUI slot. See docs/BUILDING.md.
#
# Falls back to the stock binary in .tmp_update/mainui-test/stock/MainUI when:
#   - .tmp_update/mainui-test/DISABLED exists;
#   - .tmp_update/mainui-test/MainUI is missing or not executable;
#   - Open MainUI failed twice in a row before drawing its first frame. It
#     removes $MAINUI_START_MARKER after that frame; a marker still present at
#     the next start means the previous one never got there. The fallback lasts
#     until reboot (/tmp), so a fixed card is retried.
#
# Nothing optional may stop a launcher from starting: logging is best effort,
# so a full or read-only card starts MainUI without a log.
#
# Output follows Onion's own convention: silent unless
# /mnt/SDCARD/.tmp_update/config/.logging exists, and when it does, appended to
# .tmp_update/logs/ alongside every other Onion log. Onion's runtime.sh writes
# `main 2>&1 > /dev/null`, which redirects stderr to the console rather than
# discarding it; `> /dev/null 2>&1` below is the ordering that silences both.
# This matters here because SDL's blit diagnostics go to stdout.
#
# MAINUI_WRAPPER_SD and MAINUI_WRAPPER_TMP exist for tests only.
sd=${MAINUI_WRAPPER_SD:-/mnt/SDCARD}
tmp=${MAINUI_WRAPPER_TMP:-/tmp}
sysdir="$sd/.tmp_update"
testdir="$sysdir/mainui-test"
stock="$testdir/stock/MainUI"
open="$testdir/MainUI"
marker="$tmp/open-mainui.starting"
fails_file="$tmp/open-mainui.fails"
fallback="$tmp/open-mainui.fallback"

note() {
    [ -f "$sysdir/config/.logging" ] || return 0
    { echo "$(date '+%Y-%m-%d %H:%M:%S') [wrapper] $*" >> "$sysdir/logs/MainUI.log"; } 2> /dev/null
}

run_stock() {
    if [ -x "$stock" ]; then
        exec "$stock"
    fi
    note "stock backup missing or not executable: $stock"
    if [ -x "$open" ]; then
        # Last resort: Open MainUI is better than no launcher.
        rm -f "$fallback" 2> /dev/null
        return 0
    fi
    # Nothing runnable: slow down Onion's relaunch loop instead of spinning.
    sleep 5
    exit 1
}

cd "$sd/miyoo/app" 2> /dev/null

fails=0
if [ -f "$marker" ]; then
    fails=$(cat "$fails_file" 2> /dev/null)
    case "$fails" in '' | *[!0-9]*) fails=0 ;; esac
    fails=$((fails + 1))
fi
echo "$fails" > "$fails_file" 2> /dev/null
if [ "$fails" -ge 2 ] && [ ! -f "$fallback" ]; then
    : > "$fallback" 2> /dev/null
    note "Open MainUI failed $fails starts in a row before its first frame; using stock until reboot"
fi

if [ -f "$testdir/DISABLED" ] || [ ! -x "$open" ] || [ -f "$fallback" ]; then
    rm -f "$marker" 2> /dev/null
    run_stock
fi

export LD_LIBRARY_PATH="$sysdir/lib:$sd/miyoo/lib:/config/lib:/lib"
export MAINUI_START_MARKER="$marker"
: > "$marker" 2> /dev/null

# exec and the MainUI basename retain Onion keymon's process-name contract.
if [ -f "$sysdir/config/.logging" ] && mkdir -p "$sysdir/logs" 2> /dev/null; then
    log="$sysdir/logs/MainUI.log"
    # Keep one previous session log once it reaches 1 MiB. Rotation happens
    # before exec so keymon still sees MainUI directly. A single session may
    # exceed 1 MiB.
    bytes=$(wc -c < "$log" 2> /dev/null)
    case "$bytes" in '' | *[!0-9]*) bytes=0 ;; esac
    if [ "$bytes" -ge 1048576 ]; then
        mv -f "$log" "$log.1" 2> /dev/null
    fi
    if { echo "--- MainUI start $(date '+%Y-%m-%d %H:%M:%S') ---" >> "$log"; } 2> /dev/null; then
        exec "$open" --sd-root "$sd" --device real --handoff-dir "$tmp" >> "$log" 2>&1
    fi
fi
exec "$open" --sd-root "$sd" --device real --handoff-dir "$tmp" > /dev/null 2>&1
