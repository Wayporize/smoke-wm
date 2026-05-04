#!/bin/bash

# This test is the reverse of replace-manager.sh

set -e

# Kill all child processes and remove all fifos at exit
at_exit() {
    pkill -P $$
    rm "$fifo"
}
trap at_exit INT EXIT

# Create temporary fifo
fifo="/tmp/$$.fifo"
mkfifo "$fifo"
exec 3<>"$fifo"

# Start smoke-wm
XDG_CONFIG_HOME=/tmp XDG_CONFIG_DIRS= "$SMOKE_WM" >"$fifo" &
smoke_wm_pid="$!"

wait_for_line() {
    while read -t 4 -r line ; do
        line="${line#\[*\] }"
        if [ "$line" = "$1" ] ; then
            return 0
        fi
    done < "$fifo"

    echo did not get "$1"
    return 1
}

# Wait for smoke-wm to start
wait_for_line "taking over"

# Start i3 and kill it at exit
i3 --replace >/dev/null 2>/dev/null &
i3_pid="$!"

wait_for_line "going dormant"
