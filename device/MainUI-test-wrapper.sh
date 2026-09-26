#!/bin/sh
# Runs Open MainUI in the Onion MainUI slot, falling back to the stock binary
# if it is missing or a DISABLED file is present. See docs/BUILDING.md.
#
# Output follows Onion's own convention: silent unless
# /mnt/SDCARD/.tmp_update/config/.logging exists, and when it does, appended to
# .tmp_update/logs/ alongside every other Onion log. Onion's runtime.sh writes
# `main 2>&1 > /dev/null`, which redirects stderr to the console rather than
# discarding it; `> /dev/null 2>&1` below is the ordering that silences both.
# This matters here because SDL's blit diagnostics go to stdout.
sysdir=/mnt/SDCARD/.tmp_update
testdir="$sysdir/mainui-test"

if [ -f "$testdir/DISABLED" ] || [ ! -x "$testdir/MainUI" ]; then
    exec "$testdir/stock/MainUI"
fi

export LD_LIBRARY_PATH="$sysdir/lib:/mnt/SDCARD/miyoo/lib:/config/lib:/lib"
cd /mnt/SDCARD/miyoo/app || exit 1

# exec and the MainUI basename retain Onion keymon's process-name contract.
if [ ! -f "$sysdir/config/.logging" ]; then
    exec "$testdir/MainUI" --sd-root /mnt/SDCARD --device real --handoff-dir /tmp \
        > /dev/null 2>&1
fi

log="$sysdir/logs/MainUI.log"
mkdir -p "$sysdir/logs" || exit 1
# Keep one previous session log once it reaches 1 MiB. Rotation happens before
# exec so keymon still sees MainUI directly. A single session may exceed 1 MiB.
if [ -f "$log" ]; then
    bytes=$(wc -c < "$log")
    if [ "$bytes" -ge 1048576 ]; then
        mv -f "$log" "$log.1" || exit 1
    fi
fi
echo "--- MainUI start $(date '+%Y-%m-%d %H:%M:%S') ---" >> "$log"
exec "$testdir/MainUI" --sd-root /mnt/SDCARD --device real --handoff-dir /tmp \
    >> "$log" 2>&1
